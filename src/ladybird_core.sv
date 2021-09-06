`timescale 1 ns / 1 ps

module ladybird_core
  import ladybird_config::*;
  #(parameter SIMULATION = 0)
  (
   input logic       clk,
   interface.primary inst,
   interface.primary data,
   input logic       anrst,
   input logic       nrst
   );

  function automatic logic [31:0] PC_OFFSET(logic [31:0] inst);
    return {{12{inst[31]}}, inst[19:12], inst[20], inst[30:21], 1'b0};
  endfunction

  logic [XLEN-1:0]   pc, pc_n, mmu_addr;
  logic [XLEN-1:0]   mmu_lw_data, mmu_sw_data;
  logic              mmu_req, mmu_gnt, mmu_finish;
  logic [XLEN/8-1:0] mmu_wstrb;
  logic              inst_gnt, inst_data_gnt, commit_valid, write_back;
  logic [XLEN-1:0]   inst_l, inst_data;
  logic [XLEN-1:0]   src1, src2, commit_data, immediate;
  logic [XLEN-1:0]   alu_res, alu_res_l;
  logic [4:0]        rd_addr, rs1_addr, rs2_addr;
  logic [XLEN-1:0]   rs1_data, rs2_data;

  assign inst.data = 'z;
  assign inst.wstrb = '0;
  assign inst_data_gnt = inst.data_gnt;
  assign inst_data = inst.data;
  assign inst_gnt = inst.gnt;

  typedef enum       logic [2:0] {
                                  I_FETCH,
                                  D_FETCH,
                                  EXEC,
                                  MEMORY,
                                  COMMIT
                                  } state_t;
  state_t            state, state_n;
  logic              state_progress, state_progress_n;

  logic [31:0]       gpr [32];

  ladybird_mmu MMU
    (
     .clk(clk),
     .i_valid(mmu_req),
     .i_ready(mmu_gnt),
     .i_addr(mmu_addr),
     .i_data(mmu_sw_data),
     .i_wstrb(mmu_wstrb),
     .o_valid(mmu_finish),
     .o_data(mmu_lw_data),
     .o_ready(1'b1),
     .bus(data),
     .anrst(anrst),
     .nrst(nrst)
     );

  always_comb begin
    case (state)
      I_FETCH: begin
        if (inst_gnt) begin
          state_n = D_FETCH;
          state_progress_n = 1'b1;
        end else begin
          state_n = I_FETCH;
          state_progress_n = 1'b0;
        end
      end
      D_FETCH: begin
        if (inst_data_gnt) begin
          state_n = EXEC;
          state_progress_n = 1'b1;
        end else begin
          state_n = D_FETCH;
          state_progress_n = 1'b0;
        end
      end
      EXEC: begin
        state_n = MEMORY;
        state_progress_n = 1'b1;
      end
      MEMORY: begin
        state_n = COMMIT;
        if ((inst_l[6:0] == 7'b00000_11) || (inst_l[6:0] == 7'b01000_11)) begin
          state_progress_n = mmu_req & mmu_gnt;
        end else begin
          state_progress_n = 1'b1;
        end
      end
      COMMIT: begin
        if (commit_valid) begin
          state_n = I_FETCH;
          state_progress_n = 1'b1;
        end else begin
          state_n = COMMIT;
          state_progress_n = 1'b0;
        end
      end
      default: begin
        state_n = COMMIT;
        state_progress_n = 1'b1;
      end
    endcase
  end

  always_comb begin
    if (inst_l[6:0] == 7'b11011_11) begin
      pc_n = pc + PC_OFFSET(inst_l);
    end else begin
      pc_n = pc + 'h4;
    end
  end

  always_comb begin
    rs1_addr = inst_data[19:15];
    rs2_addr = inst_data[24:20];
    //
    rd_addr = inst_l[11:7];
    if (inst_l[6:0] == 7'b01000_11) begin: BRANCH_IMMEDIATE
      immediate = {{20{inst_l[31]}}, inst_l[31:25], inst_l[11:7]};
    end else if (inst_l[6:0] == 7'b01101_11) begin: LUI_IMMEDIATE
      immediate = {inst_l[31:12], 12'h000};
    end else begin
      immediate = {{20{inst_l[31]}}, inst_l[31:20]};
    end
  end

  always_comb begin
    if (rd_addr == 5'd0) begin
      write_back = 'b0;
    end else begin
      if ((inst_l[6:0] == 7'b00000_11) || // LOAD
          (inst_l[6:0] == 7'b00100_11) || // OP_IMM
          (inst_l[6:0] == 7'b01101_11) || // LUI
          (inst_l[6:0] == 7'b01100_11) || // OP
          (inst_l[6:0] == 7'b11011_11)    // JAL
          ) begin
        write_back = 'b1;
      end else begin
        write_back = 'b0;
      end
    end
  end

  always_comb begin: ALU_SOURCE_MUX
    src1 = rs1_data;
    if ((inst_l[6:0] == 7'b00000_11) || // LOAD
        (inst_l[6:0] == 7'b00100_11) || // OP_IMM
        (inst_l[6:0] == 7'b01101_11) || // OP_LUI
        (inst_l[6:0] == 7'b01000_11)    // STORE
        ) begin
      src2 = immediate;
    end else begin: OPCODE_01100_11
      src2 = rs2_data;
    end
  end

  logic [2:0] alu_operation;
  always_comb begin: ALU_OPERATION_DECODER
    if ((inst_l[6:0] == 7'b00100_11) || (inst_l[6:0] == 7'b01100_11)) begin
      alu_operation = inst_l[14:12];
    end else begin
      alu_operation = 3'b000; // ADD
    end
  end

  logic alu_alternate;
  always_comb begin: ALTERNATE_INSTRUCTION
    if (inst_l[6:0] == 7'b00100_11) begin: operation_is_imm_arithmetic
      if (inst_l[14:12] == 3'b101) begin: operation_is_imm_shift_right
        alu_alternate = inst_l[30];
      end else begin
        alu_alternate = 1'b0;
      end
    end else if (inst_l[6:0] == 7'b01100_11) begin: operation_is_arithmetic
      alu_alternate = inst_l[30];
    end else begin
      alu_alternate = 1'b0;
    end
  end
  ladybird_alu ALU
    (
     .OPERATION(alu_operation),
     .ALTERNATE(alu_alternate),
     .SRC1(src1),
     .SRC2(src2),
     .Q(alu_res)
     );

  always_comb begin
    mmu_addr = alu_res;
    mmu_sw_data = rs2_data;
    if ((state == MEMORY) && ((inst_l[6:0] == 7'b01000_11) ||
                              (inst_l[6:0] == 7'b00000_11))) begin
      mmu_req = 'b1;
    end else begin
      mmu_req = 'b0;
    end
    if (inst_l[6:0] == 7'b01000_11) begin
      mmu_wstrb = 4'b0001;
    end else begin
      mmu_wstrb = 4'b0000;
    end
  end

  always_comb begin
    inst.addr = pc;
    if (state == I_FETCH) begin
      inst.req = 'b1;
    end else begin
      inst.req = 'b0;
    end
    if (state == COMMIT) begin
      if (inst_l[6:0] == 7'b00000_11) begin
        if (mmu_finish) begin
          commit_valid = 'b1;
        end else begin
          commit_valid = 'b0;
        end
      end else begin
        commit_valid = 'b1;
      end
    end else begin
      commit_valid = 'b0;
    end
  end

  always_comb begin
    if (inst_l[6:0] == 7'b00000_11) begin
      commit_data = mmu_lw_data;
    end else begin
      commit_data = alu_res_l;
    end
  end

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      rs1_data <= '0;
      rs2_data <= '0;
      gpr <= '{default:'0};
    end else begin
      if (~nrst) begin
        rs1_data <= '0;
        rs2_data <= '0;
        gpr <= '{default:'0};
      end else begin
        if (state == D_FETCH) begin
          if (inst_data[6:0] == 7'b01101_11) begin
            rs1_data <= '0;
          end else begin
            rs1_data <= gpr[rs1_addr];
          end
          rs2_data <= gpr[rs2_addr];
        end else if (state == COMMIT) begin
          if (write_back & commit_valid) begin
            gpr[rd_addr] <= commit_data;
          end
        end
      end
    end
  end

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      pc <= '0;
      state <= I_FETCH;
      state_progress <= 'b1;
      inst_l <= '0;
      alu_res_l <= '0;
    end else begin
      if (~nrst) begin
        pc <= '0;
        state <= I_FETCH;
        state_progress <= 'b1;
        inst_l <= '0;
        alu_res_l <= '0;
      end else begin
        if ((state == COMMIT) && commit_valid) begin
          pc <= pc_n;
        end
        state <= state_n;
        state_progress <= state_progress_n;
        if ((state == D_FETCH) && inst_data_gnt) begin
          inst_l <= inst_data;
        end
        if (state == EXEC) begin
          alu_res_l <= alu_res;
        end
      end
    end
  end

endmodule
