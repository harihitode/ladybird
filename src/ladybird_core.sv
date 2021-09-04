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
  logic              pc_increment;
  logic              inst_valid, commit_valid, write_back;
  logic [XLEN-1:0]   inst_l, inst_data;
  logic [XLEN-1:0]   src1, src2, commit_data, immediate;
  logic [XLEN-1:0]   alu_res;
  logic [4:0]        rd_addr, rs1_addr, rs2_addr;
  logic [XLEN-1:0]   rs1_data, rs2_data;

  assign inst.data = 'z;
  assign inst.wstrb = '0;
  assign inst_valid = inst.data_gnt;
  assign inst_data = inst.data;

  typedef enum       logic [1:0] {
                                  I_FETCH,
                                  D_FETCH,
                                  EXEC,
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
        if (inst_valid) begin
          state_n = D_FETCH;
          state_progress_n = 1'b1;
        end else begin
          state_n = I_FETCH;
          state_progress_n = 1'b0;
        end
      end
      D_FETCH: begin
        state_n = EXEC;
        state_progress_n = 1'b1;
      end
      EXEC: begin
        state_n = COMMIT;
        state_progress_n = 1'b1;
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
    endcase
  end

  always_comb begin
    if (state == EXEC) begin
      if (inst_l[6:0] == 7'b11011_11) begin
        pc_n = pc + PC_OFFSET(inst_l);
      end else begin
        pc_n = pc + 'h4;
      end
    end else begin
      pc_n = pc;
    end
  end

  always_comb begin
    rs1_addr = inst_l[19:15];
    rs2_addr = inst_l[24:20];
    rd_addr = inst_l[11:7];
    if (inst_l[6:0] == 7'b01000_11) begin
      immediate = {{20{inst_l[31]}}, inst_l[31:25], inst_l[11:7]};
    end else begin
      immediate = {{20{inst_l[31]}}, inst_l[31:20]};
    end
  end

  always_comb begin
    if (rd_addr == 5'd0) begin
      write_back = 'b0;
    end else begin
      if ((inst_l[6:0] == 7'b00000_11) ||
          (inst_l[6:0] == 7'b00100_11) ||
          (inst_l[6:0] == 7'b11011_11)) begin
        write_back = 'b1;
      end else begin
        write_back = 'b0;
      end
    end
  end

  always_comb begin: ALU_SOURCE_MUX
    src1 = rs1_data;
    if (inst_l[6:0] == 7'b00100_11) begin
      src2 = immediate;
    end else begin: OPCODE_01100_11
      src2 = rs2_data;
    end
  end

  logic alternate;
  always_comb begin: ALTERNATE_INSTRUCTION
    if (inst_l[6:0] == 7'b00100_11) begin: operation_is_imm_arithmetic
      if (inst_l[14:12] == 3'b101) begin: operation_is_imm_shift_right
        alternate = inst_l[30];
      end else begin
        alternate = 1'b0;
      end
    end else if (inst_l[6:0] == 7'b01100_11) begin: operation_is_arithmetic
      alternate = inst_l[30];
    end else begin
      alternate = 1'b0;
    end
  end
  ladybird_alu ALU
    (
     .OPERATION(inst_l[14:12]),
     .ALTERNATE(alternate),
     .SRC1(src1),
     .SRC2(src2),
     .Q(alu_res)
     );

  always_comb begin
    mmu_addr = rs1_data + immediate;
    mmu_sw_data = rs2_data;
    if ((state == EXEC) && ((inst_l[6:0] == 7'b01000_11) ||
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
    if (inst_valid == 'b1) begin
      pc_increment = 'b1;
    end else begin
      pc_increment = 'b0;
    end
    inst.addr = pc;
    if ((state == I_FETCH) & state_progress) begin
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
    if (inst_l[6:0] == 7'b00100_11) begin
      commit_data = alu_res;
    end else if (inst_l[6:0] == 7'b00000_11) begin
      commit_data = mmu_lw_data;
    end else begin
      commit_data = '0;
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
          rs1_data <= gpr[rs1_addr];
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
      state <= COMMIT;
      state_progress <= 'b1;
      inst_l <= '0;
    end else begin
      if (~nrst) begin
        pc <= '0;
        state <= COMMIT;
        state_progress <= 'b1;
        inst_l <= '0;
      end else begin
        pc <= pc_n;
        state <= state_n;
        state_progress <= state_progress_n;
        if ((state == I_FETCH) && inst_valid) begin
          inst_l <= inst_data;
        end
      end
    end
  end

endmodule
