`timescale 1 ns / 1 ps

package ladybird_config;
  localparam logic [31:0] VERSION = 32'h0000_0000;
  localparam              XLEN = 32;

  // riscv instruction constructor
  function automatic logic [19:0] HI(input logic [31:0] immediate);
    return immediate[31:12];
  endfunction

  function automatic logic [11:0] LO(input logic [31:0] immediate);
    return immediate[11:0];
  endfunction

  function automatic logic [31:0] LUI(input logic [4:0] rd, input logic [19:0] immediate);
    return {immediate, rd, 7'b00101_11};
  endfunction

  // OP IMM
  function automatic logic [31:0] ADDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b000, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] SLLI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b001, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] SLTI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0000000, shamt, rs, 3'b010, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] SLTIU(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b011, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] XORI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b100, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] SRLI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0000000, shamt, rs, 3'b101, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] SRAI(input logic [4:0] rd, input logic [4:0] rs, input logic [4:0] shamt);
    return {7'b0100000, shamt, rs, 3'b101, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] ORI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b110, rd, 7'b00100_11};
  endfunction

  function automatic logic [31:0] ANDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b111, rd, 7'b00100_11};
  endfunction

  // OP
  function automatic logic [31:0] ADD(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b000, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] SUB(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0100000, rs2, rs1, 3'b000, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] SLL(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b001, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] SLT(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b010, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] SLTU(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b011, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] XOR(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b100, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] SRL(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b101, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] SRA(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0100000, rs2, rs1, 3'b101, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] OR(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b110, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] AND(input logic [4:0] rd, input logic [4:0] rs1, input logic [4:0] rs2);
    return {7'b0000000, rs2, rs1, 3'b111, rd, 7'b01100_11};
  endfunction

  function automatic logic [31:0] LB(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b000, rd, 7'b00000_11};
  endfunction

  function automatic logic [31:0] SB(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b000, offset[4:0], 7'b01000_11};
  endfunction

  function automatic logic [31:0] JAL(input logic [4:0] rd, input logic [20:0] offset);
    return {offset[20], offset[10:1], offset[11], offset[19:12], rd, 7'b11011_11};
  endfunction

  // pseudo
  function automatic logic [31:0] NOP();
    return ADDI(5'd0, 5'd0, 12'd0);
  endfunction
endpackage
