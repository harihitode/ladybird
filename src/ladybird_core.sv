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

  localparam N_RMPT = 2**RMPT_W;

  // IFU I/F
  logic [XLEN-1:0]        ifu_o_inst, ifu_o_pc;
  logic                   ifu_i_valid, ifu_i_ready;
  logic                   ifu_o_valid, ifu_o_ready;
  ladybird_axi_interface #(.AXI_DATA_W(AXI_DATA_W), .AXI_ADDR_W(AXI_ADDR_W)) i_axi(.aclk(clk));

  // ALU I/F
  logic [XLEN-1:0]        alu_src1, alu_src2;
  logic [2:0]             alu_operation;
  logic                   alu_alternate;
  logic [XLEN-1:0]        alu_res;

  // CSR I/F
  logic [XLEN-1:0]        csr_src, csr_trap_code, csr_res, csr_o_pc;
  logic                   csr_o_pc_valid;

  // LSU I/F
  logic [XLEN-1:0]        lsu_data;
  logic                   lsu_req, lsu_gnt, lsu_data_valid, lsu_data_ready;
  logic                   lsu_we, lsu_fence;
  ladybird_axi_interface #(.AXI_DATA_W(AXI_DATA_W), .AXI_ADDR_W(AXI_ADDR_W)) d_axi(.aclk(clk));

  // Status
  logic                   running;
  logic                   if_ready, df_ready, ex_ready, mx_ready, wb_ready;
  logic [XLEN-1:0]        pc, npc;
  logic                   not_fall_through; // from exec_q
  // verilator lint_off UNUSED
  logic [1:0]             mode;
  logic [63:0]            instret, cycle;
  // verilator lint_on UNUSED

  typedef struct packed {
    logic [XLEN-1:0] pc;
    logic [XLEN-1:0] exception_code;
    logic [XLEN-1:0] inst;
    logic            valid;
  } i_fetch_stage_t;
  i_fetch_stage_t i_fetch_q, i_fetch_d;

  typedef struct     packed {
    logic [XLEN-1:0]   pc;
    logic [XLEN-1:0]   exception_code;
    logic [XLEN-1:0]   inst;
    logic [XLEN-1:0]   imm;
    logic [4:0]        rd_addr;
    logic [RMPT_W-1:0] table_no;
    logic              rd_wb;
    logic              rs1_valid;
    logic [XLEN-1:0]   rs1_data;
    logic              rs2_valid;
    logic [XLEN-1:0]   rs2_data;
    logic              invalidate;
    logic              valid;
  } d_fetch_stage_t;
  d_fetch_stage_t d_fetch_q, d_fetch_d;

  typedef struct packed {
    logic [XLEN-1:0]   pc;
    logic              branch_flag;
    logic              force_pc_valid;
    logic [XLEN-1:0]   force_pc;
    logic [XLEN-1:0]   exception_code;
    logic [XLEN-1:0]   inst;
    logic [XLEN-1:0]   imm;
    logic [4:0]        rd_addr;
    logic [RMPT_W-1:0] table_no;
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
    logic [RMPT_W-1:0] table_no;
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
    logic [VREG_W-1:0] rd_addr;
    logic [RMPT_W-1:0] table_no;
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

  // GENERAL PURPOSE REGISTER
  typedef struct packed {
    logic        valid;
    logic        filled;
    logic [VREG_W-1:0] vreg;
    logic [XLEN-1:0]   data;
  } remap_table_entry_t;

  logic [XLEN-1:0]     gpr [2**VREG_W];
  remap_table_entry_t  remap_table_d [2**RMPT_W];
  remap_table_entry_t  remap_table_q [2**RMPT_W];
  logic [RMPT_W-1:0]   remap_table_head, remap_table_tail;

  assign halt = ~running;
  assign not_fall_through = exec_q.valid && ~exec_q.invalidate && (exec_q.branch_flag || exec_q.force_pc_valid);

  ladybird_ifu #(.AXI_ID(BUS_ID_I), .AXI_DATA_W(AXI_DATA_W))
  IFU
    (
     .clk(clk),
     .pc(npc),
     .pc_valid(ifu_i_valid),
     .pc_ready(ifu_i_ready),
     .flush(not_fall_through),
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
     .SRC1(alu_src1),
     .SRC2(alu_src2),
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
     .o_pc(csr_o_pc),
     .o_pc_valid(csr_o_pc_valid),
     .o_trap_code(csr_trap_code),
     .nrst(nrst)
     );

  ladybird_lsu #(.AXI_ID(BUS_ID_D), .AXI_DATA_W(AXI_DATA_W))
  LSU
    (
     .clk(clk),
     .i_valid(lsu_req),
     .i_ready(lsu_gnt),
     .i_addr(exec_q.rd_data),
     .i_data(exec_q.rs2_data),
     .i_we(lsu_we),
     .i_funct(exec_q.inst[14:12]),
     .i_fence(lsu_fence),
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
    ifu_i_valid = running | (halt & resume_req);
    ifu_o_ready = df_ready;
    if (halt & resume_req) begin
      npc = resume_pc;
    end else if (not_fall_through) begin
      npc = memory_d.npc;
    end else if (ifu_i_ready) begin
      npc = pc + 'd4;
    end else begin
      npc = pc;
    end
  end

  always_comb begin
    if_ready = df_ready & running;
    if (!df_ready) begin
      i_fetch_d = i_fetch_q;
    end else begin
      if (ifu_o_valid) begin
        i_fetch_d.valid = ifu_o_valid & if_ready;
        i_fetch_d.pc = ifu_o_pc;
        i_fetch_d.inst = ifu_o_inst;
        i_fetch_d.exception_code = '0; // TODO: Instruction (Access/Page) Fault
      end else begin
        i_fetch_d = '0;
      end
    end
  end

  always_comb begin
    if (i_fetch_q.valid && riscv_dst_is_reg(i_fetch_q.inst) && remap_table_q[remap_table_head].valid) begin
      df_ready = '0;
    end else begin
      df_ready = ex_ready;
    end
    if (!ex_ready) begin
      d_fetch_d = d_fetch_q;
      d_fetch_d.invalidate = d_fetch_q.invalidate | not_fall_through;
    end else begin
      d_fetch_d.valid = i_fetch_q.valid & df_ready;
      d_fetch_d.pc = i_fetch_q.pc;
      d_fetch_d.invalidate = not_fall_through;
      d_fetch_d.inst = i_fetch_q.inst;
      d_fetch_d.exception_code = i_fetch_q.exception_code; // TODO: Illegal Instruction
      d_fetch_d.rd_addr = d_fetch_d.inst[11:7];
      d_fetch_d.rd_wb = riscv_dst_is_reg(i_fetch_q.inst);
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
      d_fetch_d.table_no = remap_table_head;
    end

    // if (riscv_src1_is_reg(d_fetch_d.inst)) begin

    // end else begin
    //   d_fetch_d.rs1_valid = '1;
    // end
    // if (riscv_src2_is_reg(d_fetch_d.inst)) begin

    // end else begin
    //   d_fetch_d.rs2_valid = '1;
    // end

    // // TODO: FIRST remap table, Second Each Pipeline, Last GPR
    d_fetch_d.rs1_valid = '1;
    if (riscv_src1_is_reg(d_fetch_d.inst)) begin
      for (int i = 0; i < N_RMPT; i++) begin
        if (remap_table_q[i].valid && remap_table_q[i].vreg == d_fetch_d.inst[19:15]) begin
          d_fetch_d.rs1_valid = '0;
          break;
        end
      end
      if (d_fetch_d.rs1_valid) begin
        d_fetch_d.rs1_data = gpr[d_fetch_d.inst[19:15]];
      end
    end
    // TODO: FIRST remap table, Second Each Pipeline, Last GPR
    d_fetch_d.rs2_valid = '1;
    if (riscv_src2_is_reg(d_fetch_d.inst)) begin
      for (int i = 0; i < N_RMPT; i++) begin
        if (remap_table_q[i].valid && remap_table_q[i].vreg == d_fetch_d.inst[24:20]) begin
          d_fetch_d.rs2_valid = '0;
          break;
        end
      end
      if (d_fetch_d.rs2_valid) begin
        d_fetch_d.rs2_data = gpr[d_fetch_d.inst[24:20]];
      end
    end
  end

  always_comb begin
    if (not_fall_through || ~d_fetch_q.valid || d_fetch_q.invalidate) begin
      ex_ready = mx_ready;
    end else begin
      ex_ready = mx_ready & d_fetch_q.valid & d_fetch_q.rs1_valid & d_fetch_q.rs2_valid;
    end
    if (!mx_ready) begin
      exec_d = exec_q;
      exec_d.invalidate = exec_q.invalidate | not_fall_through;
    end else begin
      exec_d.pc = d_fetch_q.pc;
      exec_d.exception_code = d_fetch_q.exception_code; // TODO: Illegal Instruction
      exec_d.inst = d_fetch_q.inst;
      exec_d.rd_addr = d_fetch_q.rd_addr;
      exec_d.rd_wb = d_fetch_q.rd_wb;
      exec_d.table_no = d_fetch_q.table_no;
      exec_d.imm = d_fetch_q.imm;
      exec_d.rs1_data = d_fetch_q.rs1_data;
      exec_d.rs2_data = d_fetch_q.rs2_data;
      exec_d.invalidate = d_fetch_q.invalidate | not_fall_through;
      exec_d.valid = d_fetch_q.valid & ex_ready;
      exec_d.force_pc = csr_o_pc;
      exec_d.force_pc_valid = csr_o_pc_valid;
      exec_d.branch_flag = '0;
      if (d_fetch_q.valid) begin // TODO exception
        if (d_fetch_q.inst[6:2] == OPCODE_BRANCH) begin
          if (d_fetch_q.inst[14:12] == FUNCT3_BEQ || d_fetch_q.inst[14:12] == FUNCT3_BGE || d_fetch_q.inst[14:12] == FUNCT3_BGEU) begin: BEQ_impl
            exec_d.branch_flag = ~(|alu_res);
          end else begin
            exec_d.branch_flag = |alu_res;
          end
        end else if (d_fetch_q.inst[6:2] == OPCODE_JAL || d_fetch_q.inst[6:2] == OPCODE_JALR) begin
          exec_d.branch_flag = '1;
        end
      end
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
    mx_ready = wb_ready & lsu_gnt;
    if (!wb_ready) begin
      memory_d = memory_q;
    end else begin
      memory_d.pc = exec_q.pc;
      if (exec_q.force_pc_valid) begin
        memory_d.npc = exec_q.force_pc;
      end else if (exec_q.inst[6:2] == OPCODE_JAL) begin
        memory_d.npc = exec_q.pc + exec_q.imm;
      end else if (exec_q.inst[6:2] == OPCODE_BRANCH && exec_q.branch_flag) begin
        memory_d.npc = exec_q.pc + exec_q.imm;
      end else if (exec_q.inst[6:2] == OPCODE_JALR) begin
        memory_d.npc = exec_q.rs1_data + exec_q.imm;
      end else begin
        memory_d.npc = exec_q.pc + 'd4;
      end
      memory_d.trap_code = csr_trap_code;
      memory_d.inst = exec_q.inst;
      memory_d.rd_addr = exec_q.rd_addr;
      memory_d.rd_wb = exec_q.rd_wb;
      memory_d.table_no = exec_q.table_no;
      memory_d.rs1_data = exec_q.rs1_data;
      memory_d.rs2_data = exec_q.rs2_data;
      memory_d.rd_data = exec_q.rd_data;
      memory_d.invalidate = exec_q.invalidate;
      memory_d.valid = exec_q.valid & mx_ready;
    end
  end

  always_comb begin
    if (memory_q.valid && ~memory_q.invalidate && memory_q.inst[6:2] == OPCODE_LOAD) begin
      wb_ready = lsu_data_valid & lsu_data_ready;
    end else begin
      wb_ready = '1;
    end
    commit_d.pc = memory_q.pc;
    commit_d.npc = memory_q.npc;
    commit_d.trap_code = memory_q.trap_code;
    commit_d.inst = memory_q.inst;
    commit_d.rs1_data = memory_q.rs1_data;
    commit_d.rs2_data = memory_q.rs2_data;
    commit_d.paddr = memory_q.rd_data;
    commit_d.rd_addr = memory_q.rd_addr;
    commit_d.rd_wb = memory_q.rd_wb;
    commit_d.table_no = memory_q.table_no;
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
    automatic logic [4:0] opcode = d_fetch_q.inst[6:2];
    if (opcode == OPCODE_LUI) begin: LUI_src1
      alu_src1 = '0;
    end else if ((opcode == OPCODE_AUIPC) ||
                 (opcode == OPCODE_JAL)
                 ) begin
      alu_src1 = d_fetch_q.pc;
    end else begin
      alu_src1 = d_fetch_q.rs1_data;
    end
    if ((opcode == OPCODE_LOAD) ||
        (opcode == OPCODE_OP_IMM) ||
        (opcode == OPCODE_AUIPC) ||
        (opcode == OPCODE_LUI) ||
        (opcode == OPCODE_STORE) ||
        (opcode == OPCODE_JALR) ||
        (opcode == OPCODE_JAL)
        ) begin
      alu_src2 = d_fetch_q.imm;
    end else begin: OPCODE_01100_11
      alu_src2 = d_fetch_q.rs2_data;
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
        ((exec_q.inst[6:2] == OPCODE_LOAD) || (exec_q.inst[6:2] == OPCODE_STORE) ||
         (exec_q.inst[6:2] == OPCODE_MISC_MEM))) begin
      lsu_req = 'b1;
    end else begin
      lsu_req = 'b0;
    end
    if (exec_q.inst[6:2] == OPCODE_STORE) begin
      lsu_we = 'b1;
    end else begin
      lsu_we = 'b0;
    end
    if (exec_q.inst[6:2] == OPCODE_MISC_MEM) begin
      // full fence
      lsu_fence = 'b1;
    end else begin
      lsu_fence = 'b0;
    end
    lsu_data_ready = '1;
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      gpr <= '{default:'0};
    end else begin
      if (commit_d.valid == '1 && commit_d.invalidate == '0 && commit_d.rd_wb == '1) begin
        gpr[commit_d.rd_addr] <= commit_d.rd_data;
      end
      // DEBUG
      if (commit_q.valid == '1 && commit_q.invalidate == '0) begin
        if (commit_q.rd_wb == '1 && commit_q.rd_addr == 'd2) begin
          $display("%08x (%0d) sp <- %08x", commit_q.inst, instret, commit_q.rd_data);
        end
        if (commit_q.inst[6:2] == OPCODE_LOAD && commit_q.inst[14:12] == FUNCT3_LBU) begin
          if (commit_q.rd_data[7:0] == '0) begin
            $display("%08x LB: (%08x) \\0", commit_q.pc, commit_q.paddr);
          end else if (commit_q.rd_data[7:0] == 8'h0a) begin
            $display("%08x LB: (%08x) \\n", commit_q.pc, commit_q.paddr);
          end else begin
            $display("%08x LB: (%08x) %c", commit_q.pc, commit_q.paddr, commit_q.rd_data[7:0]);
          end
        end
        if (commit_q.inst[6:2] == OPCODE_STORE && commit_q.inst[14:12] == FUNCT3_SB) begin
          if (commit_q.rs2_data[7:0] == '0) begin
            $display("%08x SB: (%08x) \\0", commit_q.pc, commit_q.paddr);
          end else if (commit_q.rs2_data[7:0] == 8'h0a) begin
            $display("%08x SB: (%08x) \\n", commit_q.pc, commit_q.paddr);
          end else begin
            $display("%08x SB: (%08x) %c", commit_q.pc, commit_q.paddr, commit_q.rs2_data[7:0]);
          end
        end
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      remap_table_head <= '0;
      remap_table_tail <= '0;
    end else begin
      // TODO: invalidate
      if (i_fetch_q.valid && riscv_dst_is_reg(i_fetch_q.inst) && df_ready) begin
        remap_table_head <= remap_table_head + 'd1;
      end
      if (commit_q.valid && commit_q.rd_wb && commit_q.table_no == remap_table_tail) begin
        remap_table_tail <= remap_table_tail + 'd1;
      end
    end
  end

  generate for (genvar i = 0; i < N_RMPT; i++) begin: GPR_REMAP_FF
    always_comb begin
      remap_table_d[i] = remap_table_q[i];
      if (i_fetch_q.valid && riscv_dst_is_reg(i_fetch_q.inst) && df_ready && i == remap_table_head) begin
        // allocation
        remap_table_d[i].valid = '1;
        remap_table_d[i].filled = '0;
        remap_table_d[i].vreg = d_fetch_d.rd_addr;
        remap_table_d[i].data = '0;
      // end else if (exec_q.valid && ~exec_q.invalidate && exec_q.rd_wb && remap_table_q[i].valid && i == exec_q.table_no && remap_table_q[i].vreg == exec_q.rd_addr && ex_ready) begin
      //   // EX -> forwarding
      //   remap_table_d[i].filled = '1;
      //   remap_table_d[i].data = exec_q.rd_data;
      // end else if (memory_q.valid && ~memory_q.invalidate && memory_q.rd_wb && remap_table_q[i].valid && i == memory_q.table_no && remap_table_q[i].vreg == memory_q.rd_addr && mx_ready) begin
      //   // MX -> forwarding
      //   remap_table_d[i].filled = '1;
      //   remap_table_d[i].data = commit_d.rd_data;
      end else if (commit_d.valid && ~commit_d.invalidate && commit_d.rd_wb &&
                   remap_table_q[i].valid && i == commit_d.table_no &&
                   remap_table_q[i].vreg == commit_d.rd_addr) begin
        // free
        remap_table_d[i] = '0;
      end
    end

    always_ff @(posedge clk) begin
      if (~nrst) begin
        remap_table_q[i] <= '0;
      end else begin
        remap_table_q[i] <= remap_table_d[i];
      end
    end
  end endgenerate

  // pipeline
  always_ff @(posedge clk) begin
    if (~nrst) begin
      running <= '0;
      pc <= '0;
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
      pc <= npc;
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
        $display($time, " %0d %08x, %08x, %s: next %08x %08x %08x %08x", rtc, commit_q.pc, commit_q.inst, ladybird_riscv_helper::riscv_disas(commit_q.inst, commit_q.pc), commit_q.npc, commit_q.rs1_data, commit_q.rs2_data, commit_q.rd_data);
`endif
      end
    end
  end

endmodule
