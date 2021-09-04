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

  function automatic logic [31:0] ADDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b000, rd, 7'b00100_11};
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
