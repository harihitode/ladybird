`timescale 1 ns / 1 ps

module ladybird_core
  import ladybird_config::*;
  #(parameter SIMULATION = 0,
    parameter logic [XLEN-1:0] TVEC = 'd0)
  (
   input logic            clk,
   interface.primary i_bus,
   interface.primary d_bus,
   input logic            start,
   input logic [XLEN-1:0] start_pc,
   input logic            pending,
   output logic           complete,
   input logic            anrst,
   input logic            nrst
   );

  logic [XLEN-1:0]        pc, pc_n, mmu_addr;
  logic [XLEN-1:0]        mmu_lw_data, mmu_sw_data;
  logic                   mmu_req, mmu_gnt, mmu_finish;
  logic                   mmu_we;
  logic                   pc_valid, pc_ready;
  logic                   inst_valid, commit_valid, write_back;
  logic [XLEN-1:0]        inst, inst_l;
  logic [XLEN-1:0]        src1, src2, commit_data, immediate;
  logic [2:0]             alu_operation;
  logic                   alu_alternate;
  logic [XLEN-1:0]        alu_res, alu_res_l;
  logic                   branch_flag;
  logic [4:0]             rd_addr, rs1_addr, rs2_addr;
  logic [XLEN-1:0]        rs1_data, rs2_data;

  typedef enum            logic [2:0] {
                                       IDLE, // initial state
                                       I_FETCH,
                                       D_FETCH,
                                       EXEC,
                                       MEMORY,
                                       COMMIT
                                       } state_t;
  state_t                 state, state_n;
  logic                   state_progress, state_progress_n;

  // GENERAL PURPOSE REGISTER
  logic [XLEN-1:0]        gpr [32];

  // SYSTEM TRAP CONTROL
  logic                   trap_occur, trap_ret, requested_trap;
  logic [XLEN-1:0]        EPC; // exception program counter
  logic                   IE; // interrupt enable
  assign trap_occur = IE & (requested_trap | pending);

  ladybird_mmu MMU
    (
     .clk(clk),
     .i_valid(mmu_req),
     .i_ready(mmu_gnt),
     .i_addr(mmu_addr),
     .i_data(mmu_sw_data),
     .i_we(mmu_we),
     .i_funct(inst_l[14:12]),
     .o_valid(mmu_finish),
     .o_data(mmu_lw_data),
     .o_ready(1'b1),
     .d_bus(d_bus),
     .i_bus(i_bus),
     .pc(pc),
     .pc_valid(pc_valid),
     .pc_ready(pc_ready),
     .inst(inst),
     .inst_valid(inst_valid),
     .anrst(anrst),
     .nrst(nrst)
     );

  always_comb begin
    case (state)
      I_FETCH: begin
        if (pc_valid & pc_ready) begin
          state_n = D_FETCH;
          state_progress_n = 1'b1;
        end else begin
          state_n = I_FETCH;
          state_progress_n = 1'b0;
        end
      end
      D_FETCH: begin
        if (inst_valid) begin
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
        if ((inst_l[6:0] == OPCODE_LOAD) || (inst_l[6:0] == OPCODE_STORE)) begin
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
        if (start) begin
          state_n = I_FETCH;
          state_progress_n = 1'b1;
        end else begin
          state_n = IDLE;
          state_progress_n = 1'b0;
        end
      end
    endcase
  end

  always_comb begin: SELECT_NEXT_PC
    case (inst_l[6:0])
      OPCODE_BRANCH: begin
        pc_n = branch_flag ? pc + immediate : pc + 'h4;
      end
      OPCODE_JALR, OPCODE_JAL: begin
        pc_n = alu_res_l;
      end
      default: begin
        pc_n = pc + 'h4;
      end
    endcase
  end

  always_comb begin
    rs1_addr = inst[19:15];
    rs2_addr = inst[24:20];
    rd_addr = inst_l[11:7];
    if (inst_l[6:0] == OPCODE_STORE) begin: STORE_OFFSET
      immediate = {{20{inst_l[31]}}, inst_l[31:25], inst_l[11:7]};
    end else if ((inst_l[6:0] == OPCODE_AUIPC) ||
                 (inst_l[6:0] == OPCODE_LUI)) begin: AUIPC_LUI_IMMEDIATE
      immediate = {inst_l[31:12], 12'h000};
    end else if (inst_l[6:0] == OPCODE_JAL) begin: JAL_OFFSET
      immediate = {{12{inst_l[31]}}, inst_l[19:12], inst_l[20], inst_l[30:21], 1'b0};
    end else if (inst_l[6:0] == OPCODE_BRANCH) begin: BRANCH_OFFSET
      immediate = {{19{inst_l[31]}}, inst_l[31], inst_l[7], inst_l[30:25], inst_l[11:8], 1'b0};
    end else begin
      immediate = {{20{inst_l[31]}}, inst_l[31:20]};
    end
  end

  always_comb begin
    if (rd_addr == 5'd0) begin
      write_back = 'b0;
    end else begin
      if ((inst_l[6:0] == OPCODE_LOAD) ||
          (inst_l[6:0] == OPCODE_OP_IMM) ||
          (inst_l[6:0] == OPCODE_AUIPC) ||
          (inst_l[6:0] == OPCODE_LUI) ||
          (inst_l[6:0] == OPCODE_OP) ||
          (inst_l[6:0] == OPCODE_JALR) ||
          (inst_l[6:0] == OPCODE_JAL)
          ) begin
        write_back = 'b1;
      end else begin
        // FENCE is treated as a NOP
        // Other opcodes have no effect for their commit
        write_back = 'b0;
      end
    end
  end

  always_comb begin: ALU_SOURCE_MUX
    if (inst_l[6:0] == OPCODE_LUI) begin: LUI_src1
      src1 = '0;
    end else if ((inst_l[6:0] == OPCODE_AUIPC) ||
                 (inst_l[6:0] == OPCODE_JAL)
                 ) begin
      src1 = pc;
    end else begin
      src1 = rs1_data;
    end
    if ((inst_l[6:0] == OPCODE_LOAD) ||
        (inst_l[6:0] == OPCODE_OP_IMM) ||
        (inst_l[6:0] == OPCODE_AUIPC) ||
        (inst_l[6:0] == OPCODE_LUI) ||
        (inst_l[6:0] == OPCODE_STORE) ||
        (inst_l[6:0] == OPCODE_JALR) ||
        (inst_l[6:0] == OPCODE_JAL)
        ) begin
      src2 = immediate;
    end else begin: OPCODE_01100_11
      src2 = rs2_data;
    end
  end

  always_comb begin: ALU_OPERATION_DECODER
    if ((inst_l[6:0] == OPCODE_OP_IMM) || (inst_l[6:0] == OPCODE_OP)) begin
      alu_operation = inst_l[14:12];
    end else if (inst_l[6:0] == OPCODE_BRANCH) begin
      if ((inst_l[14:12] == 3'b000) || (inst_l[14:12] == 3'b001)) begin: BEQ_BNE__XOR
        alu_operation = 3'b100;
      end else if ((inst_l[14:12] == 3'b100) || (inst_l[14:12] == 3'b101)) begin: BLT_BGE__SLT
        alu_operation = 3'b010;
      end else begin: BLTU_BGEU__SLTU
        alu_operation = 3'b011;
      end
    end else begin
      alu_operation = 3'b000; // default operation: ADD
    end
  end

  always_comb begin: ALTERNATE_INSTRUCTION
    if (inst_l[6:0] == OPCODE_OP_IMM) begin: operation_is_imm_arithmetic
      if (inst_l[14:12] == 3'b101) begin: operation_is_imm_shift_right
        alu_alternate = inst_l[30];
      end else begin
        alu_alternate = 1'b0;
      end
    end else if (inst_l[6:0] == OPCODE_OP) begin: operation_is_arithmetic
      alu_alternate = inst_l[30];
    end else begin
      alu_alternate = 1'b0;
    end
  end
  ladybird_alu #(.USE_FA_MODULE(0))
  ALU
    (
     .OPERATION(alu_operation),
     .ALTERNATE(alu_alternate),
     .SRC1(src1),
     .SRC2(src2),
     .Q(alu_res)
     );

  always_comb begin
    mmu_addr = alu_res_l;
    mmu_sw_data = rs2_data;
    if ((state == MEMORY) && ((inst_l[6:0] == OPCODE_LOAD) ||
                              (inst_l[6:0] == OPCODE_STORE))) begin
      mmu_req = 'b1;
    end else begin
      mmu_req = 'b0;
    end
    if (inst_l[6:0] == OPCODE_STORE) begin
      mmu_we = 'b1;
    end else begin
      mmu_we = 'b0;
    end
  end

  always_comb begin: branch_impls
    if (inst_l[14:12] == 3'b000) begin: BEQ_impl
      branch_flag = ~(|alu_res_l);
    end else if (inst_l[14:12] == 3'b001) begin: BNE_impl
      branch_flag = |alu_res_l;
    end else if ((inst_l[14:12] == 3'b101) || (inst_l[14:12] == 3'b111)) begin: BGE_BGEU_is_NOT
      branch_flag = ~alu_res_l[0];
    end else begin
      branch_flag = alu_res_l[0];
    end
  end

  always_comb begin
    if (state == I_FETCH) begin
      pc_valid = 'b1;
    end else begin
      pc_valid = 'b0;
    end
    if (state == COMMIT) begin
      if (inst_l[6:0] == OPCODE_LOAD) begin
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
    if (inst_l[6:0] == OPCODE_LOAD) begin
      commit_data = mmu_lw_data;
    end else if ((inst_l[6:0] == OPCODE_JALR) ||
                 (inst_l[6:0] == OPCODE_JAL)
                 ) begin
      commit_data = pc + 'h4; // return address for link register
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
      state <= IDLE;
      state_progress <= 'b0;
      inst_l <= '0;
      alu_res_l <= '0;
    end else begin
      if (~nrst) begin
        pc <= '0;
        state <= IDLE;
        state_progress <= 'b0;
        inst_l <= '0;
        alu_res_l <= '0;
      end else begin
        if ((state == IDLE) && start) begin
          pc <= start_pc;
        end else if (commit_valid) begin
          if (trap_ret) begin
            pc <= EPC;
          end else if (trap_occur) begin
            pc <= TVEC;
          end else begin
            pc <= pc_n;
          end
        end
        state <= state_n;
        state_progress <= state_progress_n;
        if ((state == D_FETCH) && inst_valid) begin
          inst_l <= inst;
        end
        if (state == EXEC) begin
          alu_res_l <= alu_res;
        end
      end
    end
  end

  // SYSTEM STATUS REGISTERS
  assign complete = trap_ret;

  always_comb begin: REQUESTED_TRAP_BY_SOFTWARE
    if (inst_l[6:0] == OPCODE_SYSTEM) begin
      if ((inst_l[24:20] == 5'd0) || (inst_l[24:20] == 5'd1)) begin
        requested_trap = 'b1; // ECALL and EBREAK
      end else begin
        requested_trap = 'b0; // Other System Function
      end
    end else begin
      requested_trap = 'b0;
    end
  end

  always_comb begin
    if ((inst_l[6:0] == OPCODE_SYSTEM) && (inst_l[24:20] == 5'd2)) begin
      trap_ret = 'b1;
    end else begin
      trap_ret = 'b0;
    end
  end

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      IE <= 'b1;
      EPC <= '0;
    end else begin
      if (~nrst) begin
        IE <= 'b1;
        EPC <= '0;
      end else begin
        if (commit_valid & trap_occur) begin
          EPC <= pc_n;
          IE <= 'b0;
        end else if (commit_valid & trap_ret) begin
          IE <= 'b1;
        end
      end
    end
  end

  generate if (SIMULATION) begin: INSTRUCTION_DUMPER
    always_ff @(posedge clk) begin
      automatic string nm;
      case (inst[6:0])
        OPCODE_LOAD: nm = "LOAD";
        OPCODE_MISC_MEM: nm = "MISC-MEM";
        OPCODE_OP_IMM: nm = "OP-IMM";
        OPCODE_AUIPC: nm = "AUIPC";
        OPCODE_STORE: nm = "STORE";
        OPCODE_OP: nm = "OP";
        OPCODE_LUI: nm = "LUI";
        OPCODE_BRANCH: nm = "BRANCH";
        OPCODE_JALR: nm = "JALR";
        OPCODE_JAL: nm = "JAL";
        OPCODE_SYSTEM: nm = "SYSTEM";
        default: nm = "unknown op";
      endcase
      if ((state == D_FETCH) && inst_valid) begin
        $display($time, " %08x, %08x, %s", pc, inst, nm);
      end
      if ((mmu_req) && (mmu_addr == 32'h9000_FFA0)) begin
        $display($time, " BREAK POINT, %08x, %08x, %x", mmu_lw_data, mmu_sw_data, mmu_we);
        $finish;
      end
      if ((state == COMMIT) && requested_trap) begin
        $display($time, " REQUESTED TRAP");
        $finish;
      end
    end
  end endgenerate

endmodule
