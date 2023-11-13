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
  logic [XLEN-1:0]        csr_src, csr_trap_code, csr_res;

  logic [XLEN-1:0]        src1, src2;
  logic                   idle;
  logic                   pipeline_stall;

  // Status
  // verilator lint_off UNUSED
  logic [1:0]             mode;
  commit_t                commit;
  // verilator lint_on UNUSED
  logic [XLEN-1:0]        force_pc;
  logic                   force_pc_valid;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic            valid;
  } i_fetch_stage_t;
  i_fetch_stage_t i_fetch_q, i_fetch_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] exception_code;
    logic            valid;
  } d_fetch_stage_t;
  d_fetch_stage_t d_fetch_q, d_fetch_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] exception_code;
    logic [XLEN-1:0] inst;
    logic [XLEN-1:0] imm;
    logic [XLEN-1:0] rs1_data;
    logic [XLEN-1:0] rs2_data;
    logic            valid;
  } exec_stage_t;
  exec_stage_t exec_q, exec_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] npc;
    logic [XLEN-1:0] trap_code;
    logic [XLEN-1:0] inst;
    logic [XLEN-1:0] rs1_data;
    logic [XLEN-1:0] rs2_data;
    logic [XLEN-1:0] rd_data;
    logic            valid;
  } memory_stage_t;
  memory_stage_t memory_q, memory_d;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] npc;
    logic [XLEN-1:0] trap_code;
    logic [XLEN-1:0] inst;
    logic [XLEN-1:0] rs1_data;
    logic [XLEN-1:0] rs2_data;
    logic [XLEN-1:0] paddr;
    logic [4:0]      rd_addr;
    logic [XLEN-1:0] rd_data;
    logic            valid;
  } commit_stage_t;
  commit_stage_t commit_q, commit_d;

  // GENERAL PURPOSE REGISTER
  logic [XLEN-1:0]        gpr [32];
  // verilator lint_off UNUSED
  logic [XLEN-1:0]        pc, npc, target_pc;
  logic                   branch_flag;
  logic [4:0]             stage_valid;
  logic [4:0]             stage_invalidate;
  logic [63:0]            instret, cycle;
  // verilator lint_on UNUSED

  assign pipeline_stall = '0;
  assign stage_valid = {commit_q.valid, memory_q.valid, exec_q.valid, d_fetch_q.valid, i_fetch_q.valid};
  assign idle = ~(|stage_valid);
  assign stage_invalidate = '0;

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
     .instret(instret),
     .cycle(cycle),
     .mode(mode),
     .i_req(exec_q.valid),
     .i_inst(exec_q.inst),
     .i_pc(exec_q.pc),
     .i_exception_code(exec_q.exception_code),
     .i_data(csr_src),
     .o_data(csr_res),
     .o_pc(force_pc),
     .o_trap_code(csr_trap_code),
     .o_pc_valid(force_pc_valid),
     .nrst(nrst)
     );

  ladybird_mmu
  MMU
    (
     .clk(clk),
     .i_valid(mmu_req),
     .i_ready(mmu_gnt),
     .i_addr(memory_q.rd_data),
     .i_data(memory_q.rs2_data),
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
    i_fetch_d.pc = npc;
    if ((idle & start) || commit.valid || (i_fetch_q.valid && mmu_pc_valid && ~mmu_pc_ready)) begin
      i_fetch_d.valid = '1;
    end else begin
      i_fetch_d.valid = '0;
    end
  end

  always_comb begin
    d_fetch_d.pc = i_fetch_q.pc;
    d_fetch_d.exception_code = '0; // TODO: Instruction (Access/Page) Fault
    d_fetch_d.valid = mmu_pc_valid & mmu_pc_ready;
    if (d_fetch_q.valid & ~mmu_inst_valid) begin
      d_fetch_d = d_fetch_q;
    end
  end

  always_comb begin
    automatic logic [XLEN-1:0] inst = mmu_inst;
    exec_d.pc = d_fetch_q.pc;
    exec_d.exception_code = d_fetch_q.exception_code; // TODO: Illegal Instruction
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
    memory_d.npc = npc;
    memory_d.trap_code = csr_trap_code;
    memory_d.inst = exec_q.inst;
    memory_d.rs1_data = exec_q.rs1_data;
    memory_d.rs2_data = exec_q.rs2_data;
    if (exec_q.inst[6:2] == OPCODE_SYSTEM) begin
      memory_d.rd_data = csr_res;
    end else begin
      memory_d.rd_data = alu_res;
    end
    memory_d.valid = exec_q.valid;
  end

  always_comb begin
    if (commit_q.valid && (commit_q.inst[6:2] == OPCODE_LOAD) && ~mmu_finish) begin
      commit_d = commit_q;
    end else begin
      commit_d.pc = memory_q.pc;
      commit_d.npc = memory_q.npc;
      commit_d.trap_code = memory_q.trap_code;
      commit_d.inst = memory_q.inst;
      commit_d.rs1_data = memory_q.rs1_data;
      commit_d.rs2_data = memory_q.rs2_data;
      commit_d.paddr = memory_q.rd_data;
      commit_d.rd_addr = memory_q.inst[11:7];
      commit_d.rd_data = memory_q.rd_data;
      if ((memory_q.inst[6:2] == OPCODE_LOAD) || (memory_q.inst[6:2] == OPCODE_STORE)) begin
        commit_d.valid = memory_q.valid & mmu_req & mmu_gnt;
      end else begin
        commit_d.valid = memory_q.valid;
      end
    end
  end

  always_comb begin: ALU_SOURCE_MUX
    automatic logic [4:0] opcode = exec_q.inst[6:2];
    if (opcode == OPCODE_LUI) begin: LUI_src1
      src1 = '0;
    end else if ((opcode == OPCODE_AUIPC) ||
                 (opcode == OPCODE_JAL)
                 ) begin
      src1 = exec_q.pc;
    end else begin
      src1 = exec_q.rs1_data;
    end
    if ((opcode == OPCODE_LOAD) ||
        (opcode == OPCODE_OP_IMM) ||
        (opcode == OPCODE_AUIPC) ||
        (opcode == OPCODE_LUI) ||
        (opcode == OPCODE_STORE) ||
        (opcode == OPCODE_JALR) ||
        (opcode == OPCODE_JAL)
        ) begin
      src2 = exec_q.imm;
    end else begin: OPCODE_01100_11
      src2 = exec_q.rs2_data;
    end
  end

  always_comb begin
    if (exec_q.inst[6:2] == OPCODE_BRANCH || exec_q.inst[6:2] == OPCODE_JAL) begin
      target_pc = exec_q.pc + exec_q.imm;
    end else begin
      target_pc = exec_q.rs1_data + exec_q.imm;
    end
    branch_flag = '0;
    if (exec_q.valid) begin
      if (exec_q.inst[6:2] == OPCODE_BRANCH) begin
        if (exec_q.inst[14:12] == FUNCT3_BEQ || exec_q.inst[14:12] == FUNCT3_BGE || exec_q.inst[14:12] == FUNCT3_BGEU) begin: BEQ_impl
          branch_flag = ~(|alu_res);
        end else begin
          branch_flag = |alu_res;
        end
      end else if (exec_q.inst[6:2] == OPCODE_JAL || exec_q.inst[6:2] == OPCODE_JALR) begin
        branch_flag = '1;
      end
    end
    if (idle & start) begin
      npc = start_pc;
    end else if (force_pc_valid) begin
      npc = force_pc;
    end else if (exec_q.valid) begin
      if (branch_flag) begin
        npc = target_pc;
      end else begin
        npc = pc + 'd4;
      end
    end else begin
      npc = pc;
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      pc <= '0;
    end else begin
      pc <= npc;
    end
  end

  always_comb begin: ALU_OPERATION_DECODER
    automatic logic [4:0] opcode = exec_q.inst[6:2];
    automatic logic [2:0] funct3 = exec_q.inst[14:12];
    if ((opcode == OPCODE_OP_IMM) || (opcode == OPCODE_OP)) begin
      alu_operation = funct3;
    end else if (opcode == OPCODE_BRANCH) begin
      if ((funct3 == FUNCT3_BEQ) || (funct3 == FUNCT3_BNE)) begin
        alu_operation = FUNCT3_XOR;
      end else if ((funct3 == FUNCT3_BLT) || (funct3 == FUNCT3_BGE)) begin
        alu_operation = FUNCT3_SLT;
      end else begin: BLTU_BGEU__SLTU
        alu_operation = FUNCT3_SLTU;
      end
    end else begin
      alu_operation = 3'b000; // default operation: ADD
    end
    if (opcode == OPCODE_OP_IMM) begin: operation_is_imm_arithmetic
      if (funct3 == FUNCT3_SRA) begin: operation_is_imm_shift_right
        alu_alternate = exec_q.inst[30];
      end else begin
        alu_alternate = 1'b0;
      end
    end else if (opcode == OPCODE_OP) begin: operation_is_arithmetic
      alu_alternate = exec_q.inst[30];
    end else begin
      alu_alternate = 1'b0;
    end
  end

  always_comb begin
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
    commit.trap_code = commit_q.trap_code;
    commit.pc = commit_q.pc;
    commit.npc = commit_q.npc;
    commit.inst = commit_q.inst;
    commit.paddr = commit_q.paddr;
    commit.rs1_data = commit_q.rs1_data;
    commit.rs2_data = commit_q.rs2_data;
    if (commit_q.inst[6:2] == OPCODE_LOAD) begin
      commit.wb_data = mmu_lw_data;
    end else if ((commit_q.inst[6:2] == OPCODE_JALR) ||
                 (commit_q.inst[6:2] == OPCODE_JAL)
                 ) begin
      commit.wb_data = commit_q.pc + 'h4; // return address for link register
    end else begin
      commit.wb_data = commit_q.rd_data;
    end
    if (commit_q.rd_addr == 5'd0) begin
      commit.wb_en = 'b0;
    end else begin
      if ((commit_q.inst[6:2] == OPCODE_LOAD) ||
          (commit_q.inst[6:2] == OPCODE_OP_IMM) ||
          (commit_q.inst[6:2] == OPCODE_AUIPC) ||
          (commit_q.inst[6:2] == OPCODE_LUI) ||
          (commit_q.inst[6:2] == OPCODE_OP) ||
          (commit_q.inst[6:2] == OPCODE_JALR) ||
          (commit_q.inst[6:2] == OPCODE_JAL)
          ) begin
        commit.wb_en = 'b1;
      end else if (commit_q.inst[6:2] == OPCODE_SYSTEM && commit_q.inst[14:12] != 'd0) begin
        commit.wb_en = 'b1;
      end else begin
        // FENCE is treated as a NOP
        // Other opcodes have no effect for their commit
        commit.wb_en = 'b0;
      end
    end
    if (commit_q.valid == '1) begin
      if (commit_q.inst[6:2] == OPCODE_LOAD) begin
        commit.valid = mmu_finish;
      end else begin
        commit.valid = '1;
      end
    end else begin
      commit.valid = '0;
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      gpr <= '{default:'0};
    end else begin
      if (commit_q.valid == '1 && (commit.valid & commit.wb_en)) begin
        gpr[commit_q.rd_addr] <= commit.wb_data;
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

  always_ff @(posedge clk) begin
    if (~nrst) begin
      instret <= '0;
      cycle <= '0;
    end else begin
      cycle <= cycle + 'd1;
      if (commit.valid) begin
        instret <= instret + 'd1;
`ifdef LADYBIRD_SIMULATION_DEBUG_DUMP
        $display($time, " %0d %08x, %08x, %s", rtc, commit.pc, commit.inst, ladybird_riscv_helper::riscv_disas(commit.inst, commit.pc));
        // $display("(PA)%08x (S1)%08x (S2)%08x (D)%08x (EN)%08x (NPC)%08x %c", commit.paddr, commit.rs1_data, commit.rs2_data, commit.wb_data, commit.wb_en, commit.npc, commit.wb_data[7:0]);
`endif
      end
    end
  end

endmodule
