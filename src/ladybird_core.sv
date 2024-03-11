`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_riscv_helper.svh"

module ladybird_core
  import ladybird_config::*;
  import ladybird_riscv_helper::*;
  #(parameter logic [XLEN-1:0] HART_ID = 'd0,
    parameter AXI_DATA_W = ladybird_config::XLEN,
    parameter AXI_ADDR_W = ladybird_config::XLEN
    )
  (
   input logic            clk,
   ladybird_axi_interface.master axi,
   input logic            halt_req,
   input logic            resume_req,
   input logic [XLEN-1:0] resume_pc,
   input logic [63:0]     rtc,
   output logic           halt,
   input logic            nrst
   );

  // IFU I/F
  logic [XLEN-1:0]        ifu_o_inst, ifu_o_pc;
  logic                   ifu_i_valid, ifu_i_ready;
  logic                   ifu_o_valid, ifu_o_ready;
  ladybird_axi_interface #(.AXI_DATA_W(AXI_DATA_W), .AXI_ADDR_W(AXI_ADDR_W)) i_axi(.aclk(clk));

  // ALU I/F
  logic [2:0]             alu_operation;
  logic                   alu_alternate;
  logic [XLEN-1:0]        alu_res;

  // CSR I/F
  logic [XLEN-1:0]        csr_src, csr_trap_code, csr_res;
  logic [XLEN-1:0]        src1, src2;

  // LSU I/F
  logic [XLEN-1:0]        lsu_data;
  logic                   lsu_req, lsu_gnt, lsu_data_valid, lsu_data_ready;
  logic                   lsu_we;
  ladybird_axi_interface #(.AXI_DATA_W(AXI_DATA_W), .AXI_ADDR_W(AXI_ADDR_W)) d_axi(.aclk(clk));

  // Status
  // verilator lint_off UNUSED
  logic [1:0]             mode;
  logic                   exe_busy = '0;
  // verilator lint_on UNUSED
  logic [XLEN-1:0]        force_pc;
  logic                   force_pc_valid;
  logic                   if_ready, df_ready, ex_ready, mx_ready, wb_ready;

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic            valid;
  } i_fetch_stage_t;
  i_fetch_stage_t i_fetch_q, i_fetch_d;

  typedef struct     packed {
    logic [XLEN-1:0]   pc;
    logic [XLEN-1:0]   exception_code;
    logic [XLEN-1:0]   inst;
    logic [XLEN-1:0]   imm;
    logic [4:0]        rd_addr;
    logic [PREG_W-1:0] rd_pno;
    logic              rd_wb;
    logic [XLEN-1:0]   rs1_data;
    logic [XLEN-1:0]   rs2_data;
    logic              valid;
  } d_fetch_stage_t;
  d_fetch_stage_t d_fetch_q, d_fetch_d;

  typedef struct packed {
    logic [XLEN-1:0]   pc;
    logic [XLEN-1:0]   exception_code;
    logic [XLEN-1:0]   inst;
    logic [XLEN-1:0]   imm;
    logic [4:0]        rd_addr;
    logic [PREG_W-1:0] rd_pno;
    logic              rd_wb;
    logic [XLEN-1:0]   rs1_data;
    logic [XLEN-1:0]   rs2_data;
    logic [XLEN-1:0]   rd_data;
    logic              invalidate;
    logic              valid;
  } exec_stage_t;
  exec_stage_t exec_q, exec_d;

  typedef struct packed {
    logic [XLEN-1:0]   pc;
    logic [XLEN-1:0]   npc;
    logic [XLEN-1:0]   trap_code;
    logic [XLEN-1:0]   inst;
    logic [4:0]        rd_addr;
    logic [PREG_W-1:0] rd_pno;
    logic              rd_wb;
    logic [XLEN-1:0]   rs1_data;
    logic [XLEN-1:0]   rs2_data;
    logic [XLEN-1:0]   rd_data;
    logic              invalidate;
    logic              valid;
  } memory_stage_t;
  memory_stage_t memory_q, memory_d;

  typedef struct packed {
    logic [XLEN-1:0]   pc;
    logic [XLEN-1:0]   npc;
    logic [XLEN-1:0]   trap_code;
    logic [XLEN-1:0]   inst;
    logic [4:0]        rd_addr;
    logic [PREG_W-1:0] rd_pno;
    logic              rd_wb;
    logic [XLEN-1:0]   rs1_data;
    logic [XLEN-1:0]   rs2_data;
    logic [XLEN-1:0]   paddr;
    logic [XLEN-1:0]   rd_data;
    logic              invalidate;
    logic              valid;
  } commit_stage_t;
  // verilator lint_off UNUSED
  commit_stage_t commit_q, commit_d;
  // verilator lint_on UNUSED

  logic              running;
  // GENERAL PURPOSE REGISTER
  localparam VREG_W = 5;
  localparam PREG_W = 3;
  typedef struct     packed {
    logic              valid;
    logic [PREG_W-1:0] pno;
  } gpr_remap_t;
  typedef struct       packed {
    logic              valid;
    logic [XLEN-1:0]   data;
  } phys_reg_t;
  logic [XLEN-1:0]     gpr [2**VREG_W];
  gpr_remap_t          gpr_remap [2**VREG_W];
  phys_reg_t           physreg [2**PREG_W];
  logic [PREG_W-1:0]   physreg_head;
  // verilator lint_off UNUSED
  logic [4:0]          stage_valid;
  logic [63:0]         instret, cycle;
  // verilator lint_on UNUSED
  logic [XLEN-1:0]     npc, target_pc;
  logic                branch_flag;

  assign stage_valid = {commit_q.valid, memory_q.valid, exec_q.valid, d_fetch_q.valid, i_fetch_q.valid};
  assign ifu_i_valid = running | (halt & resume_req);
  assign ifu_o_ready = df_ready;
  assign lsu_data_ready = '1;
  assign halt = ~running;

  ladybird_ifu #(.AXI_ID('d0), .AXI_DATA_W(AXI_DATA_W))
  IFU
    (
     .clk(clk),
     .pc(i_fetch_d.pc),
     .pc_valid(ifu_i_valid),
     .pc_ready(ifu_i_ready),
     .inst(ifu_o_inst),
     .inst_valid(ifu_o_valid),
     .inst_ready(ifu_o_ready),
     .inst_pc(ifu_o_pc),
     .i_axi(i_axi),
     .nrst(nrst)
     );

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
     .i_req(d_fetch_q.valid),
     .i_inst(d_fetch_q.inst),
     .i_pc(d_fetch_q.pc),
     .i_exception_code(exec_q.exception_code),
     .i_data(csr_src),
     .o_data(csr_res),
     .o_pc(force_pc),
     .o_trap_code(csr_trap_code),
     .o_pc_valid(force_pc_valid),
     .nrst(nrst)
     );

  ladybird_lsu #(.AXI_ID('d1), .AXI_DATA_W(AXI_DATA_W))
  LSU
    (
     .clk(clk),
     .i_valid(lsu_req),
     .i_ready(lsu_gnt),
     .i_addr(exec_q.rd_data),
     .i_data(exec_q.rs2_data),
     .i_we(lsu_we),
     .i_funct(exec_q.inst[14:12]),
     .o_valid(lsu_data_valid),
     .o_data(lsu_data),
     .o_ready(lsu_data_ready),
     .d_axi(d_axi),
     .nrst(nrst)
     );

  ladybird_axi_arbiter
  AXI_ARB
    (
     .i_axi_0(i_axi),
     .i_axi_1(d_axi),
     .o_axi(axi)
     );

  always_comb begin
    if (~i_fetch_q.valid) begin
      if_ready = ifu_i_ready;
    end else begin
      if_ready = df_ready;
    end
    if (if_ready) begin
      if (halt & resume_req) begin
        i_fetch_d.pc = resume_pc;
      end else if (force_pc_valid) begin
        i_fetch_d.pc = force_pc;
      end else if (branch_flag) begin
        i_fetch_d.pc = target_pc;
      end else if (ifu_o_valid && ifu_o_ready && i_fetch_q.pc == ifu_o_pc) begin
        i_fetch_d.pc = i_fetch_q.pc + 'd4;
      end else begin
        i_fetch_d.pc = i_fetch_q.pc;
      end
      i_fetch_d.valid = '1;
    end else begin
      i_fetch_d = i_fetch_q;
    end
  end

  always_comb begin
    automatic logic rs1_valid = '0;
    automatic logic rs2_valid = '0;
    if (branch_flag) begin
      d_fetch_d = '0;
      df_ready = '1;
    end else if (!ex_ready) begin
      d_fetch_d = d_fetch_q;
      df_ready = '0;
    end else begin
      d_fetch_d.pc = i_fetch_q.pc;
      d_fetch_d.inst = ifu_o_inst;
      d_fetch_d.exception_code = '0; // TODO: Instruction (Access/Page) Fault
      d_fetch_d.rd_addr = d_fetch_d.inst[11:7];
      if (d_fetch_d.inst[6:2] == OPCODE_STORE) begin: STORE_OFFSET
        d_fetch_d.imm = {{20{d_fetch_d.inst[31]}}, d_fetch_d.inst[31:25], d_fetch_d.inst[11:7]};
      end else if ((d_fetch_d.inst[6:2] == OPCODE_AUIPC) ||
                   (d_fetch_d.inst[6:2] == OPCODE_LUI)) begin: AUIPC_LUI_IMMEDIATE
        d_fetch_d.imm = {d_fetch_d.inst[31:12], 12'h000};
      end else if (d_fetch_d.inst[6:2] == OPCODE_JAL) begin: JAL_OFFSET
        d_fetch_d.imm = {{12{d_fetch_d.inst[31]}}, d_fetch_d.inst[19:12], d_fetch_d.inst[20], d_fetch_d.inst[30:21], 1'b0};
      end else if (d_fetch_d.inst[6:2] == OPCODE_BRANCH) begin: BRANCH_OFFSET
        d_fetch_d.imm = {{19{d_fetch_d.inst[31]}}, d_fetch_d.inst[31], d_fetch_d.inst[7], d_fetch_d.inst[30:25], d_fetch_d.inst[11:8], 1'b0};
      end else begin
        d_fetch_d.imm = {{20{d_fetch_d.inst[31]}}, d_fetch_d.inst[31:20]};
      end
      if (d_fetch_d.rd_addr == 5'd0) begin
        d_fetch_d.rd_wb = 'b0;
      end else begin
        if ((d_fetch_d.inst[6:2] == OPCODE_LOAD) ||
            (d_fetch_d.inst[6:2] == OPCODE_OP_IMM) ||
            (d_fetch_d.inst[6:2] == OPCODE_AUIPC) ||
            (d_fetch_d.inst[6:2] == OPCODE_LUI) ||
            (d_fetch_d.inst[6:2] == OPCODE_OP) ||
            (d_fetch_d.inst[6:2] == OPCODE_JALR) ||
            (d_fetch_d.inst[6:2] == OPCODE_JAL)
            ) begin
          d_fetch_d.rd_wb = 'b1;
        end else if (d_fetch_d.inst[6:2] == OPCODE_SYSTEM && d_fetch_d.inst[14:12] != 'd0) begin
          d_fetch_d.rd_wb = 'b1;
        end else begin
          // FENCE is treated as a NOP
          // Other opcodes have no effect for their commit
          d_fetch_d.rd_wb = 'b0;
        end
      end
      d_fetch_d.rd_pno = physreg_head;
      if (gpr_remap[d_fetch_d.inst[19:15]].valid == '1) begin
        // means vreg <-> preg remapping is valid
        if (physreg[gpr_remap[d_fetch_d.inst[19:15]].pno].valid == '1) begin
          rs1_valid = '1;
          d_fetch_d.rs1_data = physreg[gpr_remap[d_fetch_d.inst[19:15]].pno].data;
        end else begin
          d_fetch_d.rs1_data = '0;
        end
      end else begin
        rs1_valid = '1;
        d_fetch_d.rs1_data = gpr[d_fetch_d.inst[19:15]];
      end
      if (gpr_remap[d_fetch_d.inst[24:20]].valid == '1) begin
        // means vreg <-> preg remapping is valid
        if (physreg[gpr_remap[d_fetch_d.inst[24:20]].pno].valid == '1) begin
          rs2_valid = '1;
          d_fetch_d.rs2_data = physreg[gpr_remap[d_fetch_d.inst[24:20]].pno].data;
        end else begin
          d_fetch_d.rs2_data = '0;
        end
      end else begin
        rs2_valid = '1;
        d_fetch_d.rs2_data = gpr[d_fetch_d.inst[24:20]];
      end
      if (physreg[physreg_head].valid) begin
        df_ready = '0;
      end else begin
        df_ready = rs1_valid & rs2_valid & mx_ready;
      end
      if (i_fetch_q.valid && ifu_o_valid && i_fetch_q.pc == ifu_o_pc) begin
        d_fetch_d.valid = df_ready;
      end else begin
        d_fetch_d.valid = '0;
      end
    end
  end

  always_comb begin
    automatic logic [XLEN-1:0] inst = d_fetch_q.inst;
    if (exec_q.valid && ~exec_q.invalidate && (exec_q.inst[6:2] == OPCODE_LOAD || exec_q.inst[6:2] == OPCODE_STORE)) begin
      ex_ready = lsu_req & lsu_gnt & mx_ready;
    end else begin
      ex_ready = mx_ready;
    end
    if (!ex_ready) begin
      exec_d = exec_q;
    end else begin
      exec_d.pc = d_fetch_q.pc;
      exec_d.exception_code = d_fetch_q.exception_code; // TODO: Illegal Instruction
      exec_d.inst = inst;
      exec_d.rd_addr = d_fetch_q.rd_addr;
      exec_d.rd_wb = d_fetch_q.rd_wb;
      exec_d.rd_pno = d_fetch_q.rd_pno;
      exec_d.imm = d_fetch_q.imm;
      exec_d.rs1_data = d_fetch_q.rs1_data;
      exec_d.rs2_data = d_fetch_q.rs2_data;
      exec_d.invalidate = branch_flag;
      exec_d.valid = d_fetch_q.valid;
      if ((d_fetch_q.inst[6:2] == OPCODE_JALR) ||
          (d_fetch_q.inst[6:2] == OPCODE_JAL)) begin
        exec_d.rd_data = d_fetch_q.pc + 'h4; // return address for link register
      end else if (d_fetch_q.inst[6:2] == OPCODE_SYSTEM) begin
        exec_d.rd_data = csr_res;
      end else begin
        exec_d.rd_data = alu_res;
      end
    end
  end

  always_comb begin
    automatic logic [4:0] opcode = d_fetch_q.inst[6:2];
    if (opcode == OPCODE_LUI) begin: LUI_src1
      src1 = '0;
    end else if ((opcode == OPCODE_AUIPC) ||
                 (opcode == OPCODE_JAL)
                 ) begin
      src1 = d_fetch_q.pc;
    end else begin
      src1 = d_fetch_q.rs1_data;
    end
    if ((opcode == OPCODE_LOAD) ||
        (opcode == OPCODE_OP_IMM) ||
        (opcode == OPCODE_AUIPC) ||
        (opcode == OPCODE_LUI) ||
        (opcode == OPCODE_STORE) ||
        (opcode == OPCODE_JALR) ||
        (opcode == OPCODE_JAL)
        ) begin
      src2 = d_fetch_q.imm;
    end else begin: OPCODE_01100_11
      src2 = d_fetch_q.rs2_data;
    end
  end

  always_comb begin
    if (memory_q.valid && ~memory_q.invalidate && memory_q.inst[6:2] == OPCODE_LOAD) begin
      mx_ready = lsu_data_valid & lsu_data_ready & wb_ready;
    end else begin
      mx_ready = wb_ready;
    end
    if (!mx_ready) begin
      memory_d = memory_q;
    end else begin
      memory_d.pc = exec_q.pc;
      memory_d.npc = npc;
      memory_d.trap_code = csr_trap_code;
      memory_d.inst = exec_q.inst;
      memory_d.rd_addr = exec_q.rd_addr;
      memory_d.rd_wb = exec_q.rd_wb;
      memory_d.rd_pno = exec_q.rd_pno;
      memory_d.rs1_data = exec_q.rs1_data;
      memory_d.rs2_data = exec_q.rs2_data;
      memory_d.rd_data = exec_q.rd_data;
      memory_d.invalidate = exec_q.invalidate;
      if (~exec_q.invalidate && (exec_q.inst[6:2] == OPCODE_LOAD) || (exec_q.inst[6:2] == OPCODE_STORE)) begin
        memory_d.valid = exec_q.valid & lsu_req & lsu_gnt;
      end else begin
        memory_d.valid = exec_q.valid;
      end
    end
  end

  always_comb begin
    wb_ready = '1;
    commit_d.pc = memory_q.pc;
    commit_d.npc = memory_q.npc;
    commit_d.trap_code = memory_q.trap_code;
    commit_d.inst = memory_q.inst;
    commit_d.rs1_data = memory_q.rs1_data;
    commit_d.rs2_data = memory_q.rs2_data;
    commit_d.paddr = memory_q.rd_data;
    commit_d.rd_addr = memory_q.rd_addr;
    commit_d.rd_wb = memory_q.rd_wb;
    commit_d.rd_pno = memory_q.rd_pno;
    if (memory_q.inst[6:2] == OPCODE_LOAD) begin
      commit_d.rd_data = lsu_data;
    end else begin
      commit_d.rd_data = memory_q.rd_data;
    end
    commit_d.invalidate = memory_q.invalidate;
    if (memory_q.inst[6:2] == OPCODE_LOAD && ~memory_q.invalidate) begin
      commit_d.valid = memory_q.valid & lsu_data_valid;
    end else begin
      commit_d.valid = memory_q.valid;
    end
  end

  always_comb begin
    if (exec_q.inst[6:2] == OPCODE_BRANCH || exec_q.inst[6:2] == OPCODE_JAL) begin
      target_pc = exec_q.pc + exec_q.imm;
    end else begin
      target_pc = exec_q.rs1_data + exec_q.imm;
    end
    branch_flag = '0;
    if (exec_q.valid && ~exec_q.invalidate) begin
      if (exec_q.inst[6:2] == OPCODE_BRANCH) begin
        if (exec_q.inst[14:12] == FUNCT3_BEQ || exec_q.inst[14:12] == FUNCT3_BGE || exec_q.inst[14:12] == FUNCT3_BGEU) begin: BEQ_impl
          branch_flag = ~(|exec_q.rd_data);
        end else begin
          branch_flag = |exec_q.rd_data;
        end
      end else if (exec_q.inst[6:2] == OPCODE_JAL || exec_q.inst[6:2] == OPCODE_JALR) begin
        branch_flag = '1;
      end
    end
    if (halt & resume_req) begin
      npc = resume_pc;
    end else if (force_pc_valid) begin
      npc = force_pc;
    end else if (branch_flag) begin
      npc = target_pc;
    end else begin
      npc = exec_q.pc + 'd4;
    end
  end

  always_comb begin: ALU_OPERATION_DECODER
    automatic logic [4:0] opcode = d_fetch_q.inst[6:2];
    automatic logic [2:0] funct3 = d_fetch_q.inst[14:12];
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
        alu_alternate = d_fetch_q.inst[30];
      end else begin
        alu_alternate = 1'b0;
      end
    end else if (opcode == OPCODE_OP) begin: operation_is_arithmetic
      alu_alternate = d_fetch_q.inst[30];
    end else begin
      alu_alternate = 1'b0;
    end
  end

  always_comb begin
    if (d_fetch_q.inst[14]) begin
      csr_src = {{27{1'b0}}, d_fetch_q.inst[19:15]};
    end else begin
      csr_src = exec_d.rs1_data;
    end
  end

  always_comb begin
    if ((exec_q.valid == '1) && (exec_q.invalidate == '0) &&
        ((exec_q.inst[6:2] == OPCODE_LOAD) || (exec_q.inst[6:2] == OPCODE_STORE))) begin
      lsu_req = 'b1;
    end else begin
      lsu_req = 'b0;
    end
    if (exec_q.inst[6:2] == OPCODE_STORE) begin
      lsu_we = 'b1;
    end else begin
      lsu_we = 'b0;
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      gpr <= '{default:'0};
    end else begin
      if (commit_q.valid == '1 && commit_q.invalidate == '0 && commit_q.rd_wb == '1 && physreg[commit_q.rd_pno].valid) begin
        gpr[commit_q.rd_addr] <= physreg[commit_q.rd_pno].data;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      physreg_head <= '0;
    end else begin
      if (d_fetch_d.valid && d_fetch_d.rd_wb && df_ready) begin
        physreg_head <= physreg_head + 'd1;
      end
    end
  end

  generate for (genvar i = 0; i < 2**PREG_W; i++) begin: PREG_FF
    always_ff @(posedge clk) begin
      if (~nrst) begin
        physreg[i] <= '0;
      end else begin
        if (wb_ready && commit_q.valid && commit_q.rd_wb && i == commit_q.rd_pno) begin
          physreg[i] <= '0;
        end else if (ex_ready && exec_q.valid && i == exec_q.rd_pno && exec_q.inst[6:2] != OPCODE_LOAD && exec_q.rd_wb) begin
          physreg[i].valid <= '1;
          physreg[i].data <= memory_d.rd_data;
        end else if (mx_ready && memory_q.valid && i == memory_q.rd_pno && memory_q.inst[6:2] == OPCODE_LOAD && memory_q.rd_wb) begin
          physreg[i].valid <= '1;
          physreg[i].data <= commit_d.rd_data;
        end
      end
    end
  end endgenerate

  generate for (genvar i = 0; i < 2**VREG_W; i++) begin: GPR_REMAP_FF
    always_ff @(posedge clk) begin
      if (~nrst) begin
        gpr_remap[i] <= '0;
      end else begin
        if (d_fetch_d.valid && d_fetch_d.rd_wb && d_fetch_d.rd_addr == i) begin
          gpr_remap[i].valid <= '1;
          gpr_remap[i].pno <= d_fetch_d.rd_pno;
        end else if (gpr_remap[i].valid && i == commit_q.rd_addr && commit_q.rd_wb && commit_q.valid == '1 && commit_q.rd_pno == gpr_remap[i].pno) begin
          gpr_remap[i] <= '0;
        end
      end
    end
  end endgenerate

  // pipeline
  always_ff @(posedge clk) begin
    if (~nrst) begin
      running <= '0;
      i_fetch_q <= '0;
      d_fetch_q <= '0;
      exec_q <= '0;
      memory_q <= '0;
      commit_q <= '0;
    end else begin
      if (halt & resume_req) begin
        running <= '1;
      end else if (halt_req) begin
        running <= '0;
      end
      i_fetch_q <= i_fetch_d;
      d_fetch_q <= d_fetch_d;
      exec_q <= exec_d;
      memory_q <= memory_d;
      commit_q <= commit_d;
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      instret <= '0;
      cycle <= '0;
    end else begin
      cycle <= cycle + 'd1;
      if (commit_q.valid && ~commit_q.invalidate) begin
        instret <= instret + 'd1;
`ifdef LADYBIRD_SIMULATION_DEBUG_DUMP
        $display($time, " %0d %08x, %08x, %s: next %08x", rtc, commit_q.pc, commit_q.inst, ladybird_riscv_helper::riscv_disas(commit_q.inst, commit_q.pc), commit_q.npc);
`endif
      end
    end
  end

endmodule
