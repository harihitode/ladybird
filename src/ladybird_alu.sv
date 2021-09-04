`timescale 1 ns / 1 ps

module ladybird_alu
  import ladybird_config::*;
  (
   input logic [2:0]       OPERATION,
   input logic             ALTERNATE,
   input logic [XLEN-1:0]  SRC1,
   input logic [XLEN-1:0]  SRC2,
   output logic [XLEN-1:0] Q
   );
  logic [XLEN-1:0]         Q_ADD, Q_SHIFT_RIGHT, Q_SHIFT_LEFT;
  logic [0:4][XLEN-1:0]    SRC1_SHL;
  logic [0:4][XLEN-1:0]    SRC1_SHR;

  // decoding flag
  logic                    sra_flag, sub_flag;

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

  always_comb begin
    sra_flag = ALTERNATE;
    sub_flag = ALTERNATE;
  end

  always_comb begin: EXEC_AND_RESULT_MUX
    case (OPERATION)
      OP_BITWISE_AND: Q = SRC1 & SRC2;
      OP_BITWISE_OR:  Q = SRC1 | SRC2;
      OP_BITWISE_XOR: Q = SRC1 ^ SRC2;
      OP_SHIFT_LEFT:  Q = Q_SHIFT_LEFT;
      OP_SHIFT_RIGHT: Q = Q_SHIFT_RIGHT;
      OP_ADDITION:    Q = Q_ADD;
      default:        Q = '0;
    endcase
  end

  always_comb begin: ADDER
    Q_ADD = SRC1 + SRC2;
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
      SRC1_SHR[0] = {{16{SRC1[31] & sra_flag}}, SRC1[31:16]};
    end else begin
      SRC1_SHR[0] = SRC1;
    end
    if (SRC2[3]) begin
      SRC1_SHR[1] = {{8{SRC1[31] & sra_flag}}, SRC1_SHR[0][31:8]};
    end else begin
      SRC1_SHR[1] = SRC1_SHR[0];
    end
    if (SRC2[2]) begin
      SRC1_SHR[2] = {{4{SRC1[31] & sra_flag}}, SRC1_SHR[1][31:4]};
    end else begin
      SRC1_SHR[2] = SRC1_SHR[1];
    end
    if (SRC2[1]) begin
      SRC1_SHR[3] = {{2{SRC1[31] & sra_flag}}, SRC1_SHR[2][31:2]};
    end else begin
      SRC1_SHR[3] = SRC1_SHR[2];
    end
    if (SRC2[0]) begin
      SRC1_SHR[4] = {{1{SRC1[31] & sra_flag}}, SRC1_SHR[3][31:1]};
    end else begin
      SRC1_SHR[4] = SRC1_SHR[3];
    end
    Q_SHIFT_RIGHT = SRC1_SHR[4];
  end

endmodule
