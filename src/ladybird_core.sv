`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_riscv_helper.svh"

module ladybird_core
  import ladybird_config::*;
  import ladybird_riscv_helper::*;
  #(parameter logic [XLEN-1:0] HART_ID = 'd0)
  (
   input logic            clk,
   ladybird_axi_interface.master axi,
   input logic            start,
   input logic [XLEN-1:0] start_pc,
   input logic [63:0]     rtc,
   output logic           trap,
   input logic            nrst
   );

  // ALU I/F
  logic [2:0]             alu_operation;
  logic                   alu_alternate;
  logic [XLEN-1:0]        alu_res;

  // MMU I/F
  logic [XLEN-1:0]        mmu_inst, mmu_lw_data;
  logic                   mmu_req, mmu_gnt, mmu_finish;
  logic                   mmu_we;
  logic                   mmu_pc_valid, mmu_pc_ready;
  logic                   mmu_inst_valid;

  // CSR I/F
  logic                   csr_req;
  logic [XLEN-1:0]        csr_src, csr_res;
  logic [11:0]            csr_addr;

  logic                   retire, writeback_valid;
  logic [XLEN-1:0]        src1, src2, writeback_data;
  logic                   idle;
  logic                   pipeline_stall;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic            valid;
  } i_fetch_stage_t;
  i_fetch_stage_t i_fetch_q, i_fetch_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic            valid;
  } d_fetch_stage_t;
  d_fetch_stage_t d_fetch_q, d_fetch_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] inst;
    logic [XLEN-1:0] imm;
    logic [XLEN-1:0] rs1_data;
    logic [XLEN-1:0] rs2_data;
    logic            valid;
  } exec_stage_t;
  exec_stage_t exec_q, exec_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] inst;
    logic [XLEN-1:0] imm;
    logic [XLEN-1:0] addr;
    logic [XLEN-1:0] data;
    logic [XLEN-1:0] rs1_data;
    logic [XLEN-1:0] rd_data;
    logic            valid;
  } memory_stage_t;
  memory_stage_t memory_q, memory_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] inst;
    logic [4:0]      rd_addr;
    logic [XLEN-1:0] branch_pc;
    logic            branch_flag;
    logic [XLEN-1:0] rd_data;
    logic            valid;
  } commit_stage_t;
  commit_stage_t commit_q, commit_d;

  // GENERAL PURPOSE REGISTER
  logic [XLEN-1:0]        gpr [32];
  // verilator lint_off UNUSED
  logic [XLEN-1:0]        pc;
  logic [4:0]             stage_valid;
  // verilator lint_on UNUSED

  // SYSTEM Operation flag
  logic                   ebreak, ecall, mret, sret;

  assign trap = ebreak | ecall | mret | sret;
  assign idle = ~(i_fetch_q.valid | d_fetch_q.valid | exec_q.valid | memory_q.valid | commit_q.valid);
  assign pipeline_stall = '0;
  assign stage_valid[0] = i_fetch_q.valid;
  assign stage_valid[1] = d_fetch_q.valid;
  assign stage_valid[2] = exec_q.valid;
  assign stage_valid[3] = memory_q.valid;
  assign stage_valid[4] = commit_q.valid;

  ladybird_alu #(.USE_FA_MODULE(0))
  ALU
    (
     .OPERATION(alu_operation),
     .ALTERNATE(alu_alternate),
     .SRC1(src1),
     .SRC2(src2),
     .Q(alu_res)
     );

  ladybird_csr #(.HART_ID(HART_ID))
  CSR
    (
     .clk(clk),
     .rtc(rtc),
     .retire(retire),
     .retire_pc(commit_q.pc),
     .retire_inst(commit_q.inst),
     .retire_next_pc(i_fetch_d.pc),
     .i_op(exec_q.inst[14:12]),
     .i_valid(csr_req),
     .i_addr(csr_addr),
     .i_data(csr_src),
     .o_data(csr_res),
     .nrst(nrst)
     );

  ladybird_mmu
  MMU
    (
     .clk(clk),
     .i_valid(mmu_req),
     .i_ready(mmu_gnt),
     .i_addr(memory_q.addr),
     .i_data(memory_q.data),
     .i_we(mmu_we),
     .i_funct(memory_q.inst[14:12]),
     .o_valid(mmu_finish),
     .o_data(mmu_lw_data),
     .o_ready(1'b1),
     .axi(axi),
     .pc(i_fetch_q.pc),
     .pc_valid(mmu_pc_valid),
     .pc_ready(mmu_pc_ready),
     .inst(mmu_inst),
     .inst_valid(mmu_inst_valid),
     .nrst(nrst)
     );

  always_comb begin
    d_fetch_d.pc = i_fetch_q.pc;
    d_fetch_d.valid = mmu_pc_valid & mmu_pc_ready;
    if (d_fetch_q.valid & ~mmu_inst_valid) begin
      d_fetch_d = d_fetch_q;
    end
  end

  always_comb begin
    automatic logic [XLEN-1:0] inst = mmu_inst;
    exec_d.pc = d_fetch_q.pc;
    exec_d.inst = inst;
    if (inst[6:2] == OPCODE_STORE) begin: STORE_OFFSET
      exec_d.imm = {{20{inst[31]}}, inst[31:25], inst[11:7]};
    end else if ((inst[6:2] == OPCODE_AUIPC) ||
                 (inst[6:2] == OPCODE_LUI)) begin: AUIPC_LUI_IMMEDIATE
      exec_d.imm = {inst[31:12], 12'h000};
    end else if (inst[6:2] == OPCODE_JAL) begin: JAL_OFFSET
      exec_d.imm = {{12{inst[31]}}, inst[19:12], inst[20], inst[30:21], 1'b0};
    end else if (inst[6:2] == OPCODE_BRANCH) begin: BRANCH_OFFSET
      exec_d.imm = {{19{inst[31]}}, inst[31], inst[7], inst[30:25], inst[11:8], 1'b0};
    end else begin
      exec_d.imm = {{20{inst[31]}}, inst[31:20]};
    end
    exec_d.rs1_data = gpr[inst[19:15]];
    exec_d.rs2_data = gpr[inst[24:20]];
    exec_d.valid = mmu_inst_valid;
  end

  always_comb begin
    memory_d.pc = exec_q.pc;
    memory_d.inst = exec_q.inst;
    memory_d.imm = exec_q.imm;
    memory_d.addr = exec_q.rs1_data + exec_q.imm;
    memory_d.data = exec_q.rs2_data;
    if (exec_q.inst[6:2] == OPCODE_SYSTEM) begin
      memory_d.rd_data = csr_res;
    end else begin
      memory_d.rd_data = alu_res;
    end
    memory_d.rs1_data = exec_q.rs1_data;
    memory_d.valid = exec_q.valid;
  end

  always_comb begin
    if (commit_q.valid && (commit_q.inst[6:2] == OPCODE_LOAD) && ~mmu_finish) begin
      commit_d = commit_q;
    end else begin
      commit_d.pc = memory_q.pc;
      commit_d.inst = memory_q.inst;
      commit_d.rd_addr = memory_q.inst[11:7];
      if (memory_q.inst[14:12] == 3'b000) begin: BEQ_impl
        commit_d.branch_flag = ~(|memory_q.rd_data);
      end else if (memory_q.inst[14:12] == 3'b001) begin: BNE_impl
        commit_d.branch_flag = |memory_q.rd_data;
      end else if ((memory_q.inst[14:12] == 3'b101) || (memory_q.inst[14:12] == 3'b111)) begin: BGE_BGEU_is_NOT
        commit_d.branch_flag = ~memory_q.rd_data[0];
      end else begin
        commit_d.branch_flag = memory_q.rd_data[0];
      end
      commit_d.rd_data = memory_q.rd_data;
      if ((memory_q.inst[6:2] == OPCODE_LOAD) || (memory_q.inst[6:2] == OPCODE_STORE)) begin
        commit_d.valid = memory_q.valid & mmu_req & mmu_gnt;
      end else begin
        commit_d.valid = memory_q.valid;
      end
      if (memory_q.inst[6:2] == OPCODE_BRANCH || memory_q.inst[6:2] == OPCODE_JAL) begin
        commit_d.branch_pc = memory_q.pc + memory_q.imm;
      end else begin
        commit_d.branch_pc = memory_q.rs1_data + memory_q.imm;
      end
    end
  end

  always_comb begin
    if (idle & start) begin
      i_fetch_d.pc = start_pc;
      i_fetch_d.valid = '1;
    end else begin
      i_fetch_d = i_fetch_q;
      if (commit_q.valid) begin
        case (commit_q.inst[6:2])
          OPCODE_BRANCH: begin
            i_fetch_d.pc = commit_q.branch_flag ? commit_q.branch_pc : commit_q.pc + 'h4;
          end
          OPCODE_JALR, OPCODE_JAL: begin
            i_fetch_d.pc = commit_q.branch_pc;
          end
          default: begin
            i_fetch_d.pc = commit_q.pc + 'h4;
          end
        endcase
        i_fetch_d.valid = retire;
      end else if (i_fetch_q.valid & mmu_pc_valid & mmu_pc_ready) begin
        i_fetch_d.valid = '0;
      end
    end
  end

  always_comb begin: ALU_SOURCE_MUX
    if (exec_q.inst[6:2] == OPCODE_LUI) begin: LUI_src1
      src1 = '0;
    end else if ((exec_q.inst[6:2] == OPCODE_AUIPC) ||
                 (exec_q.inst[6:2] == OPCODE_JAL)
                 ) begin
      src1 = exec_q.pc;
    end else begin
      src1 = exec_q.rs1_data;
    end
    if ((exec_q.inst[6:2] == OPCODE_LOAD) ||
        (exec_q.inst[6:2] == OPCODE_OP_IMM) ||
        (exec_q.inst[6:2] == OPCODE_AUIPC) ||
        (exec_q.inst[6:2] == OPCODE_LUI) ||
        (exec_q.inst[6:2] == OPCODE_STORE) ||
        (exec_q.inst[6:2] == OPCODE_JALR) ||
        (exec_q.inst[6:2] == OPCODE_JAL)
        ) begin
      src2 = exec_q.imm;
    end else begin: OPCODE_01100_11
      src2 = exec_q.rs2_data;
    end
  end

  always_comb begin: ALU_OPERATION_DECODER
    if ((exec_q.inst[6:2] == OPCODE_OP_IMM) || (exec_q.inst[6:2] == OPCODE_OP)) begin
      alu_operation = exec_q.inst[14:12];
    end else if (exec_q.inst[6:2] == OPCODE_BRANCH) begin
      if ((exec_q.inst[14:12] == 3'b000) || (exec_q.inst[14:12] == 3'b001)) begin: BEQ_BNE__XOR
        alu_operation = 3'b100;
      end else if ((exec_q.inst[14:12] == 3'b100) || (exec_q.inst[14:12] == 3'b101)) begin: BLT_BGE__SLT
        alu_operation = 3'b010;
      end else begin: BLTU_BGEU__SLTU
        alu_operation = 3'b011;
      end
    end else begin
      alu_operation = 3'b000; // default operation: ADD
    end
    if (exec_q.inst[6:2] == OPCODE_OP_IMM) begin: operation_is_imm_arithmetic
      if (exec_q.inst[14:12] == 3'b101) begin: operation_is_imm_shift_right
        alu_alternate = exec_q.inst[30];
      end else begin
        alu_alternate = 1'b0;
      end
    end else if (exec_q.inst[6:2] == OPCODE_OP) begin: operation_is_arithmetic
      alu_alternate = exec_q.inst[30];
    end else begin
      alu_alternate = 1'b0;
    end
  end

  always_comb begin
    if (exec_q.valid == '1 && exec_q.inst[6:2] == OPCODE_SYSTEM && exec_q.inst[14:12] != 'd0) begin
      csr_req = '1;
    end else begin
      csr_req = '0;
    end
    csr_addr = exec_q.inst[31:20];
    if (exec_q.inst[14]) begin
      csr_src = {{27{1'b0}}, exec_q.inst[19:15]};
    end else begin
      csr_src = exec_q.rs1_data;
    end
  end

  always_comb begin
    if ((memory_q.valid == '1) && ((memory_q.inst[6:2] == OPCODE_LOAD) ||
                                   (memory_q.inst[6:2] == OPCODE_STORE))) begin
      mmu_req = 'b1;
    end else begin
      mmu_req = 'b0;
    end
    if (memory_q.inst[6:2] == OPCODE_STORE) begin
      mmu_we = 'b1;
    end else begin
      mmu_we = 'b0;
    end
  end

  always_comb begin
    if (i_fetch_q.valid == '1) begin
      mmu_pc_valid = 'b1;
    end else begin
      mmu_pc_valid = 'b0;
    end
  end

  always_comb begin
    if (commit_q.valid == '1) begin
      if (commit_q.inst[6:2] == OPCODE_LOAD) begin
        if (mmu_finish) begin
          retire = 'b1;
        end else begin
          retire = 'b0;
        end
      end else begin
        retire = 'b1;
      end
    end else begin
      retire = 'b0;
    end
  end

  always_comb begin
    if (commit_q.inst[6:2] == OPCODE_LOAD) begin
      writeback_data = mmu_lw_data;
    end else if ((commit_q.inst[6:2] == OPCODE_JALR) ||
                 (commit_q.inst[6:2] == OPCODE_JAL)
                 ) begin
      writeback_data = commit_q.pc + 'h4; // return address for link register
    end else begin
      writeback_data = commit_q.rd_data;
    end
  end

  always_comb begin
    if (commit_q.rd_addr == 5'd0) begin
      writeback_valid = 'b0;
    end else begin
      if ((commit_q.inst[6:2] == OPCODE_LOAD) ||
          (commit_q.inst[6:2] == OPCODE_OP_IMM) ||
          (commit_q.inst[6:2] == OPCODE_AUIPC) ||
          (commit_q.inst[6:2] == OPCODE_LUI) ||
          (commit_q.inst[6:2] == OPCODE_OP) ||
          (commit_q.inst[6:2] == OPCODE_JALR) ||
          (commit_q.inst[6:2] == OPCODE_JAL)
          ) begin
        writeback_valid = 'b1;
      end else begin
        // FENCE is treated as a NOP
        // Other opcodes have no effect for their commit
        writeback_valid = 'b0;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      gpr <= '{default:'0};
    end else begin
      if (commit_q.valid == '1 && (writeback_valid & retire)) begin
        if (writeback_valid & retire) begin
          gpr[commit_q.rd_addr] <= writeback_data;
        end
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      pc <= '0;
    end else begin
      if (idle && start) begin
        pc <= start_pc;
      end else if (retire) begin
        pc <= i_fetch_d.pc;
      end
    end
  end

  always_comb begin
    automatic logic [6:0] funct7 = commit_q.inst[31:25];
    automatic logic [4:0] rs2 = commit_q.inst[24:20];
    ecall = '0;
    ebreak = '0;
    mret = '0;
    sret = '0;
    if (retire && commit_q.inst[6:2] == OPCODE_SYSTEM) begin
      if (funct7 == 7'h00 && rs2 == 'd0) begin
        ecall = '1;
      end
      if (funct7 == 7'h00 && rs2 == 'd1) begin
        ebreak = '1;
      end
      if (funct7 == 7'h18 && rs2 == 'd2) begin
        mret = '1;
      end
      if (funct7 == 7'h08 && rs2 == 'd2) begin
        sret = '1;
      end
    end
  end

  // pipeline
  always_ff @(posedge clk) begin
    if (~nrst) begin
      i_fetch_q <= '0;
      d_fetch_q <= '0;
      exec_q <= '0;
      memory_q <= '0;
      commit_q <= '0;
    end else begin
      if (~pipeline_stall) begin
        i_fetch_q <= i_fetch_d;
        d_fetch_q <= d_fetch_d;
        exec_q <= exec_d;
        memory_q <= memory_d;
        commit_q <= commit_d;
      end
    end
  end

endmodule
