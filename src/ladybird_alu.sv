`timescale 1 ns / 1 ps
`include "../src/ladybird_config.svh"

module ladybird_alu
  import ladybird_config::*;
  #(parameter SIMULATION = 0,
    parameter USE_FA_MODULE = 1)
  (
   input logic [2:0]       OPERATION,
   input logic             ALTERNATE,
   input logic [XLEN-1:0]  SRC1,
   input logic [XLEN-1:0]  SRC2,
   output logic [XLEN-1:0] Q
   );
  logic [XLEN-1:0]         Q_ADD, Q_SHIFT_RIGHT, Q_SHIFT_LEFT;
  logic [4:0][XLEN-1:0]    SRC1_SHL;
  logic [4:0][XLEN-1:0]    SRC1_SHR;
  logic [XLEN-1:0]         SRC2_ALT;
  logic                    COMPARISON;
  logic                    SUBTRACT;
  logic                    CARRY, OVERFLOW;

  typedef enum             logic [2:0]
                           {
                            OP_ADDITION        = 3'b000,
                            OP_SHIFT_LEFT      = 3'b001,
                            OP_SET_LESS_THAN   = 3'b010,
                            OP_SET_LESS_THAN_U = 3'b011,
                            OP_BITWISE_XOR     = 3'b100,
                            OP_SHIFT_RIGHT     = 3'b101,
                            OP_BITWISE_OR      = 3'b110,
                            OP_BITWISE_AND     = 3'b111
                            } OPERATIONS_T;

  generate if (SIMULATION) begin: ALU_SIMULATION
    initial begin
      $display("alu: fa-module: %d", USE_FA_MODULE);
    end
  end endgenerate

  always_comb begin: EXEC_AND_RESULT_MUX
    case (OPERATION)
      OP_ADDITION:        Q = Q_ADD;
      OP_SHIFT_LEFT:      Q = Q_SHIFT_LEFT;
      OP_SET_LESS_THAN:   Q = {31'd0, OVERFLOW ^ Q_ADD[31]};
      OP_SET_LESS_THAN_U: Q = {31'd0, ~CARRY};
      OP_BITWISE_AND:     Q = SRC1 & SRC2;
      OP_BITWISE_OR:      Q = SRC1 | SRC2;
      OP_BITWISE_XOR:     Q = SRC1 ^ SRC2;
      OP_SHIFT_RIGHT:     Q = Q_SHIFT_RIGHT;
      default:            Q = '0;
    endcase
  end

  always_comb begin
    COMPARISON = OPERATION[1];
    SUBTRACT = ALTERNATE | COMPARISON;
    SRC2_ALT = SUBTRACT ? ~SRC2 : SRC2;
  end

  generate if (USE_FA_MODULE) begin: FA_ARITHMETIC
    logic [XLEN:0] CARRY_I;
    assign CARRY_I[0] = SUBTRACT;
    // adder
    for (genvar i = 0; i < XLEN; i++) begin: FULLADDER
      ladybird_full_adder ADDER
                             (
                              .x(SRC1[i]),
                              .y(SRC2_ALT[i]),
                              .c_in(CARRY_I[i]),
                              .q(Q_ADD[i]),
                              .c_out(CARRY_I[i+1])
                              );
    end
    always_comb begin
      CARRY = CARRY_I[32]; // CARRY FLAG
    end
  end else begin: ARITHMETIC
    always_comb begin
      if (SUBTRACT == 1'b1) begin
        {CARRY, Q_ADD} = SRC1 + SRC2_ALT + 32'd1;
      end else begin
        {CARRY, Q_ADD} = SRC1 + SRC2_ALT;
      end
    end
  end endgenerate

  always_comb begin
    // OVERFLOW = ~(SRC1[31] ^ SRC2_ALT[31]) & (Q_ADD[31] ^ CARRY); // OVERFLOW FLAG
    OVERFLOW = (SRC1[31] & SRC2_ALT[31] & ~Q_ADD[31]) | (~SRC1[31] & ~SRC2_ALT[31] & Q_ADD[31]); // OVERFLOW FLAG
  end

  always_comb begin: BARREL_SHIFTER_LEFT
    if (SRC2[4]) begin
      SRC1_SHL[0] = {SRC1[15:0], {16{1'b0}}};
    end else begin
      SRC1_SHL[0] = SRC1;
    end
    if (SRC2[3]) begin
      SRC1_SHL[1] = {SRC1_SHL[0][23:0], {8{1'b0}}};
    end else begin
      SRC1_SHL[1] = SRC1_SHL[0];
    end
    if (SRC2[2]) begin
      SRC1_SHL[2] = {SRC1_SHL[1][27:0], {4{1'b0}}};
    end else begin
      SRC1_SHL[2] = SRC1_SHL[1];
    end
    if (SRC2[1]) begin
      SRC1_SHL[3] = {SRC1_SHL[2][29:0], {2{1'b0}}};
    end else begin
      SRC1_SHL[3] = SRC1_SHL[2];
    end
    if (SRC2[0]) begin
      SRC1_SHL[4] = {SRC1_SHL[3][30:0], {1{1'b0}}};
    end else begin
      SRC1_SHL[4] = SRC1_SHL[3];
    end
    Q_SHIFT_LEFT = SRC1_SHL[4];
  end

  always_comb begin: BARREL_SHIFTER_RIGHT
    if (SRC2[4]) begin
      SRC1_SHR[0] = {{16{SRC1[31] & ALTERNATE}}, SRC1[31:16]};
    end else begin
      SRC1_SHR[0] = SRC1;
    end
    if (SRC2[3]) begin
      SRC1_SHR[1] = {{8{SRC1[31] & ALTERNATE}}, SRC1_SHR[0][31:8]};
    end else begin
      SRC1_SHR[1] = SRC1_SHR[0];
    end
    if (SRC2[2]) begin
      SRC1_SHR[2] = {{4{SRC1[31] & ALTERNATE}}, SRC1_SHR[1][31:4]};
    end else begin
      SRC1_SHR[2] = SRC1_SHR[1];
    end
    if (SRC2[1]) begin
      SRC1_SHR[3] = {{2{SRC1[31] & ALTERNATE}}, SRC1_SHR[2][31:2]};
    end else begin
      SRC1_SHR[3] = SRC1_SHR[2];
    end
    if (SRC2[0]) begin
      SRC1_SHR[4] = {{1{SRC1[31] & ALTERNATE}}, SRC1_SHR[3][31:1]};
    end else begin
      SRC1_SHR[4] = SRC1_SHR[3];
    end
    Q_SHIFT_RIGHT = SRC1_SHR[4];
  end

endmodule
