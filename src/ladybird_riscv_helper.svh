`ifndef LADYBIRD_RISCV_HELPER_SVH
`define LADYBIRD_RISCV_HELPER_SVH

`include "../src/ladybird_config.svh"
`timescale 1 ns / 1 ps

package ladybird_riscv_helper;
  import ladybird_config::XLEN;
  // verilator lint_off UNUSEDSIGNAL
  // verilator lint_off UNUSEDPARAM
  localparam logic [4:0] OPCODE_LOAD = 5'b00000;
  localparam logic [4:0] OPCODE_MISC_MEM = 5'b00011;
  localparam logic [4:0] OPCODE_OP_IMM = 5'b00100;
  localparam logic [4:0] OPCODE_AUIPC = 5'b00101;
  localparam logic [4:0] OPCODE_STORE = 5'b01000;
  localparam logic [4:0] OPCODE_AMO = 5'b01011;
  localparam logic [4:0] OPCODE_OP = 5'b01100;
  localparam logic [4:0] OPCODE_LUI = 5'b01101;
  localparam logic [4:0] OPCODE_BRANCH = 5'b11000;
  localparam logic [4:0] OPCODE_JALR = 5'b11001;
  localparam logic [4:0] OPCODE_JAL = 5'b11011;
  localparam logic [4:0] OPCODE_SYSTEM = 5'b11100;

  localparam logic [4:0] AMO_LR   = 5'b00010;
  localparam logic [4:0] AMO_SC   = 5'b00011;
  localparam logic [4:0] AMO_SWAP = 5'b00001;
  localparam logic [4:0] AMO_ADD  = 5'b00000;
  localparam logic [4:0] AMO_XOR  = 5'b00100;
  localparam logic [4:0] AMO_AND  = 5'b01100;
  localparam logic [4:0] AMO_OR   = 5'b01000;
  localparam logic [4:0] AMO_MIN  = 5'b10000;
  localparam logic [4:0] AMO_MAX  = 5'b10100;
  localparam logic [4:0] AMO_MINU = 5'b11000;
  localparam logic [4:0] AMO_MAXU = 5'b11100;

  localparam logic [2:0] FUNCT3_LB = 3'b000;
  localparam logic [2:0] FUNCT3_LH = 3'b001;
  localparam logic [2:0] FUNCT3_LW = 3'b010;
  localparam logic [2:0] FUNCT3_LBU = 3'b100;
  localparam logic [2:0] FUNCT3_LHU = 3'b101;
  localparam logic [2:0] FUNCT3_SB = 3'b000;
  localparam logic [2:0] FUNCT3_SH = 3'b001;
  localparam logic [2:0] FUNCT3_SW = 3'b010;

  localparam logic [2:0] FUNCT3_CSRRW = 3'b001;
  localparam logic [2:0] FUNCT3_CSRRS = 3'b010;
  localparam logic [2:0] FUNCT3_CSRRC = 3'b011;
  localparam logic [2:0] FUNCT3_CSRRWI = 3'b101;
  localparam logic [2:0] FUNCT3_CSRRSI = 3'b110;
  localparam logic [2:0] FUNCT3_CSRRCI = 3'b111;

  localparam POS_FENCE_PRED_I = 27;
  localparam POS_FENCE_PRED_O = 26;
  localparam POS_FENCE_PRED_R = 25;
  localparam POS_FENCE_PRED_W = 24;
  localparam POS_FENCE_SUCC_I = 23;
  localparam POS_FENCE_SUCC_O = 22;
  localparam POS_FENCE_SUCC_R = 21;
  localparam POS_FENCE_SUCC_W = 20;
  localparam POS_ALU_ALTERNATE = 30;
  localparam POS_AMO_AQ = 26;
  localparam POS_AMO_RL = 25;

  // CSR (Basic)
  localparam [11:0] CSR_ADDR_CYCLE = 12'hc00;
  localparam [11:0] CSR_ADDR_TIME = 12'hc01;
  localparam [11:0] CSR_ADDR_INSTRET = 12'hc02;
  localparam [11:0] CSR_ADDR_CYCLEH = 12'hc80;
  localparam [11:0] CSR_ADDR_TIMEH = 12'hc81;
  localparam [11:0] CSR_ADDR_INSTRETH = 12'hc82;

  // CSR (Priv.)
  localparam [11:0] CSR_ADDR_M_STATUS = 12'h300;
  localparam [11:0] CSR_ADDR_M_STATUSH = 12'h310;
  localparam [11:0] CSR_ADDR_M_ISA = 12'h301;
  localparam [11:0] CSR_ADDR_M_TVEC = 12'h305;
  localparam [11:0] CSR_ADDR_M_HARTID = 12'hf14;
  localparam [11:0] CSR_ADDR_M_EPC = 12'h341;
  localparam [11:0] CSR_ADDR_M_CAUSE = 12'h342;

  // S-MODE
  localparam [11:0] CSR_ADDR_S_TVEC = 12'h105;
  localparam [11:0] CSR_ADDR_S_EPC = 12'h141;
  localparam [11:0] CSR_ADDR_S_CAUSE = 12'h142;

  localparam [1:0] PRIV_MODE_M = 2'b11;
  localparam [1:0] PRIV_MODE_S = 2'b01;
  localparam [1:0] PRIV_MODE_U = 2'b00;

  // Exception Code
  localparam [XLEN-1:0] TRAP_CODE_ILLEGAL_INSTRUCTION = 32'h00000002;
  localparam [XLEN-1:0] TRAP_CODE_BREAKPOINT = 32'h00000003;
  localparam [XLEN-1:0] TRAP_CODE_ENVIRONMENT_CALL_U = 32'h00000008;
  localparam [XLEN-1:0] TRAP_CODE_ENVIRONMENT_CALL_S = 32'h00000009;
  localparam [XLEN-1:0] TRAP_CODE_ENVIRONMENT_CALL_M = 32'h0000000b;
  localparam [XLEN-1:0] TRAP_CODE_INSTRUCTION_ACCESS_FAULT = 32'h0000001;
  localparam [XLEN-1:0] TRAP_CODE_LOAD_ACCESS_FAULT = 32'h00000005;
  localparam [XLEN-1:0] TRAP_CODE_STORE_ACCESS_FAULT = 32'h00000007;
  localparam [XLEN-1:0] TRAP_CODE_AMO_ACCESS_FAULT = 32'h00000007;
  localparam [XLEN-1:0] TRAP_CODE_INSTRUCTION_PAGE_FAULT = 32'h000000c;
  localparam [XLEN-1:0] TRAP_CODE_LOAD_PAGE_FAULT = 32'h0000000d;
  localparam [XLEN-1:0] TRAP_CODE_STORE_PAGE_FAULT = 32'h0000000f;
  localparam [XLEN-1:0] TRAP_CODE_AMO_PAGE_FAULT = 32'h0000000f;

  // Interrupt Code
  localparam [XLEN-1:0] TRAP_CODE_M_EXTERNAL_INTERRUPT = 32'h8000000b;
  localparam [XLEN-1:0] TRAP_CODE_S_EXTERNAL_INTERRUPT = 32'h80000009;
  localparam [XLEN-1:0] TRAP_CODE_M_TIMER_INTERRUPT = 32'h80000007;
  localparam [XLEN-1:0] TRAP_CODE_S_TIMER_INTERRUPT = 32'h80000005;
  localparam [XLEN-1:0] TRAP_CODE_M_SOFTWARE_INTERRUPT = 32'h80000003;
  localparam [XLEN-1:0] TRAP_CODE_S_SOFTWARE_INTERRUPT = 32'h80000001;

  // riscv instruction constructor
  function automatic logic [19:0] HI(input logic [31:0] immediate);
    return immediate[31:12];
  endfunction

  function automatic logic [11:0] LO(input logic [31:0] immediate);
    return immediate[11:0];
  endfunction

  function automatic logic [31:0] AUIPC(input logic [4:0] rd, input logic [19:0] immediate);
    return {immediate, rd, OPCODE_AUIPC, 2'b11};
  endfunction

  function automatic logic [31:0] LUI(input logic [4:0] rd, input logic [19:0] immediate);
    return {immediate, rd, OPCODE_LUI, 2'b11};
  endfunction

  // OP IMM
  function automatic logic [31:0] ADDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b000, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] SLLI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b001, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] SLTI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0000000, shamt, rs, 3'b010, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] SLTIU(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b011, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] XORI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b100, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] SRLI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0000000, shamt, rs, 3'b101, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] SRAI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0100000, shamt, rs, 3'b101, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] ORI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b110, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  function automatic logic [31:0] ANDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b111, rd, OPCODE_OP_IMM, 2'b11};
  endfunction

  // OP
  function automatic logic [31:0] ADD(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b000, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] SUB(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0100000, rs2, rs1, 3'b000, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] SLL(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b001, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] SLT(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b010, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] SLTU(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b011, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] XOR(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b100, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] SRL(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b101, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] SRA(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0100000, rs2, rs1, 3'b101, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] OR(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b110, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] AND(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b111, rd, OPCODE_OP, 2'b11};
  endfunction

  function automatic logic [31:0] LB(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b000, rd, OPCODE_LOAD, 2'b11};
  endfunction

  function automatic logic [31:0] LH(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b001, rd, OPCODE_LOAD, 2'b11};
  endfunction

  function automatic logic [31:0] LW(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b010, rd, OPCODE_LOAD, 2'b11};
  endfunction

  function automatic logic [31:0] LBU(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b100, rd, OPCODE_LOAD, 2'b11};
  endfunction

  function automatic logic [31:0] LHU(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b101, rd, OPCODE_LOAD, 2'b11};
  endfunction

  function automatic logic [31:0] SB(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b000, offset[4:0], OPCODE_STORE, 2'b11};
  endfunction

  function automatic logic [31:0] SH(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b001, offset[4:0], OPCODE_STORE, 2'b11};
  endfunction

  function automatic logic [31:0] SW(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b010, offset[4:0], OPCODE_STORE, 2'b11};
  endfunction

  function automatic logic [31:0] JALR(input logic [4:0] rd, input logic [4:0] base, input logic [11:0] offset);
    return {offset[11:0], base, rd, 3'b000, OPCODE_JALR, 2'b11};
  endfunction

  function automatic logic [31:0] JAL(input logic [4:0] rd, input logic [20:0] offset);
    return {offset[20], offset[10:1], offset[11], offset[19:12], rd, OPCODE_JAL, 2'b11};
  endfunction

  function automatic logic [31:0] BEQ(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b000, offset[4:1], offset[11], OPCODE_BRANCH, 2'b11};
  endfunction

  function automatic logic [31:0] BNE(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b001, offset[4:1], offset[11], OPCODE_BRANCH, 2'b11};
  endfunction

  function automatic logic [31:0] BLT(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b100, offset[4:1], offset[11], OPCODE_BRANCH, 2'b11};
  endfunction

  function automatic logic [31:0] BGE(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b101, offset[4:1], offset[11], OPCODE_BRANCH, 2'b11};
  endfunction

  function automatic logic [31:0] BLTU(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b110, offset[4:1], offset[11], OPCODE_BRANCH, 2'b11};
  endfunction

  function automatic logic [31:0] BGEU(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b111, offset[4:1], offset[11], OPCODE_BRANCH, 2'b11};
  endfunction

  // System Functions
  function automatic logic [31:0] ECALL();
    return {12'h000, 5'd0, 3'b0, 5'd0, OPCODE_SYSTEM, 2'b11};
  endfunction

  function automatic logic [31:0] EBREAK();
    return {12'h001, 5'd0, 3'b0, 5'd0, OPCODE_SYSTEM, 2'b11};
  endfunction

  function automatic logic [31:0] MRET();
    return {12'h302, 5'd0, 3'b0, 5'd0, OPCODE_SYSTEM, 2'b11};
  endfunction

  // FENCE
  function automatic logic [31:0] FENCE(input logic [3:0] PRED, input logic [3:0] SUCC);
    automatic logic [3:0] FM = 4'b0000; // NORMAL FENCE
    return {FM, PRED, SUCC, 5'd0, 3'b000, 5'd0, OPCODE_MISC_MEM, 2'b11};
  endfunction

  // pseudo
  function automatic logic [31:0] NOP();
    return ADDI(5'd0, 5'd0, 12'd0);
  endfunction

  function automatic logic [31:0] J(input logic [20:0] offset);
    return JAL(5'd0, offset);
  endfunction

  typedef struct packed {
    // fetch
    logic [XLEN-1:0] inst;
    logic [XLEN-1:0] pc;
    logic [63:0]     cycle;
    logic [1:0]      prv;
    // decoded
    logic            compressed;
    logic [4:0]      opcode;
    logic [XLEN-1:0] pc_fallthrough;
    logic [4:0]      rd_regno;
    logic [4:0]      rs1_regno;
    logic [4:0]      rs2_regno;
    logic [31:0]     alu_imm;
    logic [4:0]      csr_imm;
    logic [11:0]     csr_addr;
    logic [XLEN-1:0] mmu_offset;
    logic [XLEN-1:0] branch_offset;
    // result
    logic [XLEN-1:0] rd_data;
    logic [XLEN-1:0] pc_next;
    logic [XLEN-1:0] pc_paddr;
    logic            mmu_flush;
    logic [1:0]      mmu_access_type;
    logic [XLEN-1:0] mmu_vaddr;
    logic [XLEN-1:0] mmu_paddr;
    logic [XLEN-1:0] mmu_data;
    // exception
    logic            trap_ret;
    logic [XLEN-1:0] exception_code;
  } result_t;

  function logic [XLEN-1:0] riscv_decompress(input result_t result);
    return result.inst; // TODO
  endfunction


  function automatic result_t riscv_decode(input result_t result);
    automatic logic [2:0] funct3;
    automatic logic [4:0] funct5;
    automatic logic [6:0] funct7;
    automatic result_t ret = result;
    ret.compressed = (result.inst[1:0] == 2'b11) ? '0 : '1;
    ret.inst = riscv_decompress(result);
    ret.opcode = ret.inst[6:2];
    ret.pc_fallthrough = (ret.compressed) ? ret.pc + 'd2 : ret.pc + 'd4;
    ret.rd_regno = ret.inst[11:7];
    ret.rs1_regno = ret.inst[19:15];
    ret.rs2_regno = ret.inst[24:20];
    funct3 = ret.inst[14:12];
    funct5 = ret.inst[31:27];
    funct7 = ret.inst[31:25];
    if (ret.opcode == OPCODE_LUI || ret.opcode == OPCODE_AUIPC) begin
      ret.alu_imm = {ret.inst[31:12], 12'h000};
    end else begin
      if (funct3 == 3'b001 || funct3 == 3'b101) begin // shift
        ret.alu_imm = {{27{1'b0}}, ret.inst[24:20]};
      end else begin
        ret.alu_imm = {{20{ret.inst[31]}}, ret.inst[31:20]};
      end
    end
    ret.csr_addr = ret.inst[31:20];
    ret.csr_imm = ret.inst[19:15];
    if (ret.opcode == OPCODE_LOAD) begin
      ret.mmu_offset = {{20{ret.inst[31]}}, ret.inst[31:20]};
    end else begin
      ret.mmu_offset = {{20{ret.inst[31]}}, ret.inst[31:25], ret.inst[11:7]};
    end
    if (ret.opcode == OPCODE_JALR) begin
      ret.branch_offset = {{20{ret.inst[31]}}, ret.inst[31:20]};
    end else if (ret.opcode == OPCODE_JAL) begin
      ret.branch_offset = {{12{ret.inst[31]}}, ret.inst[19:12], ret.inst[20], ret.inst[30:21], 1'b0};
    end else begin
      ret.branch_offset = {{19{ret.inst[31]}}, ret.inst[30], ret.inst[7], ret.inst[29:24], ret.inst[11:8], 1'b0};
    end
    return ret;
  endfunction

`ifdef LADYBIRD_SIMULATION
  function automatic string riscv_get_mnemonic(input logic [XLEN-1:0] inst);
    automatic logic [2:0] funct3 = inst[14:12];
    automatic logic [4:0] funct5 = inst[31:27];
    automatic logic [6:0] funct7 = inst[31:25];
    automatic logic [4:0] rd = inst[11:7];
    automatic logic [4:0] rs1 = inst[19:15];
    automatic logic [4:0] rs2 = inst[24:20];
    automatic logic alternate = inst[POS_ALU_ALTERNATE];
    automatic string ret;
    case (inst[6:2])
      OPCODE_OP_IMM:
        case (funct3)
          3'b000: ret = "ADDI";
          3'b001: ret = "SLLI";
          3'b010: ret = "SLTI";
          3'b011: ret = "SLTUI";
          3'b100: ret = "XORI";
          3'b101: ret = (alternate == 1'b1) ? "SRAI" : "SRLI";
          3'b110: ret = "ORI";
          3'b111: ret = "ANDI";
        endcase
      OPCODE_OP:
        if (funct7 == 1) begin
          case (funct3)
            3'b000: ret = "MUL";
            3'b001: ret = "MULH"; // MULH (extended: signed * signedb)
            3'b010: ret = "MULHSU"; // MULHSU (extended: signed * unsigned)
            3'b011: ret = "MULHU"; // MULHU (extended: unsigned * unsigned)
            3'b100: ret = "DIV";
            3'b101: ret = "DIVU";
            3'b110: ret = "REM";
            3'b111: ret = "REMU";
          endcase
        end else begin
          case (funct3)
            3'b000: ret = (alternate == 1'b1) ? "SUB" : "ADD";
            3'b001: ret = "SLL";
            3'b010: ret = "SLT";
            3'b011: ret = "SLTU";
            3'b100: ret = "XOR";
            3'b101: ret = (alternate == 1'b1) ? "SRA" : "SRL";
            3'b110: ret = "OR";
            3'b111: ret = "AND";
          endcase
        end
      OPCODE_AUIPC: ret = "AUIPC";
      OPCODE_LUI: ret = "LUI";
      OPCODE_JALR: ret = "JALR";
      OPCODE_JAL: ret = "JAL";
      OPCODE_STORE:
        case (funct3)
          3'b000: ret = "SB";
          3'b001: ret = "SH";
          3'b010: ret = "SW";
          default: ret = "ILLEGAL (OPCODE_STORE)";
        endcase
      OPCODE_LOAD:
        case (funct3)
          3'b000: ret = "LB";
          3'b001: ret = "LH";
          3'b010: ret = "LW";
          3'b100: ret = "LBU";
          3'b101: ret = "LHU";
          default: ret = "ILLEGAL (OPCODE_LOAD)";
        endcase
      OPCODE_AMO: begin
        case (funct5)
          5'b00000: ret = "AMOADD";
          5'b00001: ret = "AMOSWAP";
          5'b00010: ret = "LR";
          5'b00011: ret = "SC";
          5'b00100: ret = "AMOXOR";
          5'b01000: ret = "AMOOR";
          5'b01100: ret = "AMOAND";
          5'b10000: ret = "AMOMIN";
          5'b10100: ret = "AMOMAX";
          5'b11000: ret = "AMOMINU";
          5'b11100: ret = "AMOMAXU";
          default: ret = "ILLEGAL (OPCODE_AMO)";
        endcase
        if (inst[POS_AMO_AQ] == 1'b1) ret = {ret, ".AQ"};
        if (inst[POS_AMO_RL] == 1'b1) ret = {ret, ".RL"};
      end
      OPCODE_MISC_MEM: begin
        if (funct3 == 3'h0 && rs1 == 'd0 && rd == 'd0) begin
          if (inst[31:28] == 4'h0 && inst[27:24] != 4'h0 && inst[23:20] != 4'h0) begin
            ret = "FENCE.";
            if (inst[POS_FENCE_PRED_I] == 1'b1) ret = {ret, "I"};
            if (inst[POS_FENCE_PRED_O] == 1'b1) ret = {ret, "O"};
            if (inst[POS_FENCE_PRED_R] == 1'b1) ret = {ret, "R"};
            if (inst[POS_FENCE_PRED_W] == 1'b1) ret = {ret, "W"};
            ret = {ret, ","};
            if (inst[POS_FENCE_SUCC_I] == 1'b1) ret = {ret, "I"};
            if (inst[POS_FENCE_SUCC_O] == 1'b1) ret = {ret, "O"};
            if (inst[POS_FENCE_SUCC_R] == 1'b1) ret = {ret, "R"};
            if (inst[POS_FENCE_SUCC_W] == 1'b1) ret = {ret, "W"};
          end else if (inst[31:28] == 4'h8 && inst[27:24] == 4'h3 && inst[23:20] == 4'h3) begin
            ret = "FENCE.TSO";
          end else begin
            ret = "ILLEGAL (OPCODE_MISC_MEM)";
          end
        end else if (funct3 == 3'h1) begin
          ret = "FENCE.I";
        end else begin
          ret = "ILLEGAL (OPCODE_MISC_MEM)";
        end
      end
      OPCODE_SYSTEM:
        case (funct3)
          3'b000: begin
            if (funct7 == 7'h00 && rs2 == 'd0) begin
              ret = "ECALL";
            end else if (funct7 == 7'h00 && rs2 == 'd1) begin
              ret = "EBREAK";
            end else if (funct7 == 7'h18 && rs2 == 'd2) begin
              ret = "MRET";
            end else if (funct7 == 7'h08 && rs2 == 'd2) begin
              ret = "SRET";
            end else if (funct7 == 7'h08 && rs2 == 'd5) begin
              ret = "WFI";
            end else if (funct7 == 7'h09) begin
              ret = "SFENCE.VMA";
            end else begin
              ret = "ILLEGAL (OPCODE_SYSTEM)";
            end
          end
          3'b001: ret = "CSRRW";
          3'b010: ret = "CSRRS";
          3'b011: ret = "CSRRC";
          3'b101: ret = "CSRRWI";
          3'b110: ret = "CSRRSI";
          3'b111: ret = "CSRRCI";
          default: ret = "ILLEGAL (OPCODE_SYSTEM)";
        endcase
      OPCODE_BRANCH:
        case (funct3)
          3'b000: ret = "BEQ";
          3'b001: ret = "BNE";
          3'b100: ret = "BLT";
          3'b101: ret = "BGE";
          3'b110: ret = "BLTU";
          3'b111: ret = "BGEU";
          default: ret = "ILLEGAL (OPCODE_BRANCH)";
        endcase
      default: ret = "ILLEGAL";
    endcase
    return ret;
  endfunction

  function string riscv_disas(input logic [31:0] inst, input logic [31:0] pc);
    automatic string asm;
    automatic string mnemonic;
    automatic result_t ret;
    ret.inst = inst;
    ret.pc = pc;
    ret = riscv_decode(ret);
    mnemonic = riscv_get_mnemonic(ret.inst);
    case (ret.opcode)
      OPCODE_LOAD: asm = $sformatf("[%s] x%0d <- x%0d[0x%0x]", mnemonic, ret.rd_regno, ret.rs1_regno, ret.mmu_offset);
      OPCODE_MISC_MEM: asm = $sformatf("[%s]", mnemonic);
      OPCODE_OP_IMM: asm = $sformatf("[%s] x%0d <- x%0d, %0d", mnemonic, ret.rd_regno, ret.rs1_regno, ret.alu_imm);
      OPCODE_AUIPC: asm = $sformatf("[%s] x%0d <- 0x%0x", mnemonic, ret.rd_regno, ret.pc + ret.alu_imm);
      OPCODE_STORE: asm = $sformatf("[%s] x%0d -> x%0d[0x%0x]", mnemonic, ret.rs2_regno, ret.rs1_regno, ret.mmu_offset);
      OPCODE_OP: asm = $sformatf("[%s] x%0d <- x%0d, x%0d", mnemonic, ret.rd_regno, ret.rs1_regno, ret.rs2_regno);
      OPCODE_LUI: asm = $sformatf("[%s] x%0d <- 0x%0x", mnemonic, ret.rd_regno, ret.alu_imm);
      OPCODE_BRANCH: asm = $sformatf("[%s] x%0d, x%0d pc <- [0x%0x]", mnemonic, ret.rs1_regno, ret.rs2_regno, ret.pc + ret.branch_offset);
      OPCODE_JALR: asm = $sformatf("[%s] x%0d <- %0x pc <- [x%0d + 0x%0x]", mnemonic, ret.rd_regno, ret.pc_fallthrough, ret.rs1_regno, ret.branch_offset);
      OPCODE_JAL: asm = $sformatf("[%s] x%0d <- %0x pc <- [0x%0x]", mnemonic, ret.rd_regno, ret.pc_fallthrough, ret.pc + ret.branch_offset);
      OPCODE_SYSTEM:   begin
        case (ret.inst[14:12])
          3'b000: asm = $sformatf("[%s]", mnemonic);
          default: begin // CSR
            if (ret.inst[14]) begin
              asm = $sformatf("[%s] x%0d <-> %0d CSR[0x%0x]", mnemonic, ret.rd_regno, ret.csr_imm, ret.csr_addr);
            end else begin
              asm = $sformatf("[%s] x%0d <-> x%0d CSR[0x%0x]", mnemonic, ret.rd_regno, ret.rs1_regno, ret.csr_addr);
            end
          end
        endcase
      end
      OPCODE_AMO: begin
        case (ret.inst[31:27])
          AMO_LR: asm = $sformatf("[%s] x%0d <- x0[x%0d]", mnemonic, ret.rd_regno, ret.rs1_regno);
          AMO_SC: asm = $sformatf("[%s] x%0d -> x0[x%0d] (x%0d <- success)", mnemonic, ret.rs2_regno, ret.rs1_regno, ret.rd_regno);
          default: asm = $sformatf("[%s] x%0d <- x0[x%0d], x0[x%0d] <- x%0d", mnemonic, ret.rd_regno, ret.rs1_regno, ret.rs1_regno, ret.rs2_regno);
        endcase
      end
      default: asm = $sformatf("[unknown]");
    endcase
    return asm;
  endfunction
`endif
  // verilator lint_on UNUSEDSIGNAL
  // verilator lint_on UNUSEDPARAM
endpackage

`endif
