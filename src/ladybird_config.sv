`timescale 1 ns / 1 ps

package ladybird_config;
  localparam logic [31:0] VERSION = 32'h0000_0000;
  localparam              XLEN = 32;

  // core bus type
  typedef enum            logic [0:0] {
                                       D_BUS = 1'b0,
                                       I_BUS = 1'b1
                                       } core_bus_t;
  // access type
  typedef enum            logic [2:0] {
                                       IRAM = 3'b000,
                                       BRAM = 3'b001,
                                       DRAM = 3'b010,
                                       UART = 3'b011,
                                       GPIO = 3'b100
                                       } access_t;

  function automatic access_t ACCESS_TYPE(input logic [XLEN-1:0] addr);
    case (addr[XLEN-1-:4])
      4'hF:    return UART;
      4'hE:    return GPIO;
      4'h8:    return BRAM;
      4'h9:    return IRAM;
      default: return DRAM;
    endcase
  endfunction

  localparam logic [6:0]      OPCODE_LOAD = 7'b00000_11;
  localparam logic [6:0]      OPCODE_MISC_MEM = 7'b00011_11;
  localparam logic [6:0]      OPCODE_OP_IMM = 7'b00100_11;
  localparam logic [6:0]      OPCODE_AUIPC = 7'b00101_11;
  localparam logic [6:0]      OPCODE_STORE = 7'b01000_11;
  localparam logic [6:0]      OPCODE_OP = 7'b01100_11;
  localparam logic [6:0]      OPCODE_LUI = 7'b01101_11;
  localparam logic [6:0]      OPCODE_BRANCH = 7'b11000_11;
  localparam logic [6:0]      OPCODE_JALR = 7'b11001_11;
  localparam logic [6:0]      OPCODE_JAL = 7'b11011_11;
  localparam logic [6:0]      OPCODE_SYSTEM = 7'b11100_11;

  // riscv instruction constructor
  function automatic logic [19:0] HI(input logic [31:0] immediate);
    return immediate[31:12];
  endfunction

  function automatic logic [11:0] LO(input logic [31:0] immediate);
    return immediate[11:0];
  endfunction

  function automatic logic [31:0] AUIPC(input logic [4:0] rd, input logic [19:0] immediate);
    return {immediate, rd, OPCODE_AUIPC};
  endfunction

  function automatic logic [31:0] LUI(input logic [4:0] rd, input logic [19:0] immediate);
    return {immediate, rd, OPCODE_LUI};
  endfunction

  // OP IMM
  function automatic logic [31:0] ADDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b000, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] SLLI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b001, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] SLTI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0000000, shamt, rs, 3'b010, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] SLTIU(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b011, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] XORI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b100, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] SRLI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0000000, shamt, rs, 3'b101, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] SRAI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0100000, shamt, rs, 3'b101, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] ORI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b110, rd, OPCODE_OP_IMM};
  endfunction

  function automatic logic [31:0] ANDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b111, rd, OPCODE_OP_IMM};
  endfunction

  // OP
  function automatic logic [31:0] ADD(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b000, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] SUB(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0100000, rs2, rs1, 3'b000, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] SLL(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b001, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] SLT(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b010, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] SLTU(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b011, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] XOR(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b100, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] SRL(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b101, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] SRA(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0100000, rs2, rs1, 3'b101, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] OR(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b110, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] AND(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b111, rd, OPCODE_OP};
  endfunction

  function automatic logic [31:0] LB(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b000, rd, OPCODE_LOAD};
  endfunction

  function automatic logic [31:0] LH(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b001, rd, OPCODE_LOAD};
  endfunction

  function automatic logic [31:0] LW(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b010, rd, OPCODE_LOAD};
  endfunction

  function automatic logic [31:0] LBU(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b100, rd, OPCODE_LOAD};
  endfunction

  function automatic logic [31:0] LHU(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b101, rd, OPCODE_LOAD};
  endfunction

  function automatic logic [31:0] SB(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b000, offset[4:0], OPCODE_STORE};
  endfunction

  function automatic logic [31:0] SH(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b001, offset[4:0], OPCODE_STORE};
  endfunction

  function automatic logic [31:0] SW(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b010, offset[4:0], OPCODE_STORE};
  endfunction

  function automatic logic [31:0] JALR(input logic [4:0] rd, input logic [4:0] base, input logic [11:0] offset);
    return {offset[11:0], base, rd, 3'b000, OPCODE_JALR};
  endfunction

  function automatic logic [31:0] JAL(input logic [4:0] rd, input logic [20:0] offset);
    return {offset[20], offset[10:1], offset[11], offset[19:12], rd, OPCODE_JAL};
  endfunction

  function automatic logic [31:0] BEQ(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b000, offset[4:1], offset[11], OPCODE_BRANCH};
  endfunction

  function automatic logic [31:0] BNE(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b001, offset[4:1], offset[11], OPCODE_BRANCH};
  endfunction

  function automatic logic [31:0] BLT(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b100, offset[4:1], offset[11], OPCODE_BRANCH};
  endfunction

  function automatic logic [31:0] BGE(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b101, offset[4:1], offset[11], OPCODE_BRANCH};
  endfunction

  function automatic logic [31:0] BLTU(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b110, offset[4:1], offset[11], OPCODE_BRANCH};
  endfunction

  function automatic logic [31:0] BGEU(input logic [4:0] rs1, input logic [4:0] rs2, input logic [12:0] offset);
    return {offset[12], offset[10:5], rs2, rs1, 3'b111, offset[4:1], offset[11], OPCODE_BRANCH};
  endfunction

  // System Functions
  function automatic logic [31:0] ECALL();
    return {12'h000, 5'd0, 3'b0, 5'd0, OPCODE_SYSTEM};
  endfunction

  function automatic logic [31:0] EBREAK();
    return {12'h001, 5'd0, 3'b0, 5'd0, OPCODE_SYSTEM};
  endfunction

  function automatic logic [31:0] MRET();
    return {12'h302, 5'd0, 3'b0, 5'd0, OPCODE_SYSTEM};
  endfunction

  // FENCE
  function automatic logic [31:0] FENCE(input logic [3:0] PRED, input logic [3:0] SUCC);
    automatic logic [3:0] FM = 3'b000; // NORMAL FENCE
    return {FM, PRED, SUCC, 5'd0, 3'b000, 5'd0, OPCODE_MISC_MEM};
  endfunction

  // pseudo
  function automatic logic [31:0] NOP();
    return ADDI(5'd0, 5'd0, 12'd0);
  endfunction

  function automatic logic [31:0] J(input logic [20:0] offset);
    return JAL(5'd0, offset);
  endfunction

  function automatic logic [31:0] NOT(input logic [4:0] rd, input logic [4:0] rs);
    return XORI(rd, rs, 12'hfff);
  endfunction

endpackage
