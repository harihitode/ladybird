`timescale 1 ns / 1 ps

module ladybird_fifo
  #(parameter FIFO_DEPTH_W = 5,
    parameter DATA_W = 8)
  (
   input logic               clk,
   input logic [DATA_W-1:0]  a_data,
   input logic               a_valid,
   output logic              a_ready,
   output logic [DATA_W-1:0] b_data,
   output logic              b_valid,
   input logic               b_ready,
   input logic               nrst
   );

  localparam logic [1:0]     FIFO_DEFAULT = 2'b00;
  localparam logic [1:0]     FIFO_R = 2'b01;
  localparam logic [1:0]     FIFO_W = 2'b10;
  localparam logic [1:0]     FIFO_WR = 2'b11;

  localparam logic           NEXT_POS = (FIFO_DEPTH_W > 0) ? 1'b1 : 1'b0;

  logic [0:(2**FIFO_DEPTH_W)-1][DATA_W-1:0] mem;
  logic                                     full;
  logic [1:0]                               instruction;
  logic [((FIFO_DEPTH_W > 0) ? FIFO_DEPTH_W : 1)-1:0] read_pos, write_pos;

  assign a_ready = ~full;
  assign b_valid = (write_pos == read_pos) ? full : 1'b1;
  assign b_data = mem[read_pos];
  assign instruction = {a_valid & a_ready, b_valid & b_ready};

  always_ff @(posedge clk) begin
    if (~nrst) begin
      read_pos <= 'd0;
      write_pos <= 'd0;
      mem <= '{default:'0};
    end else begin
      if (instruction & FIFO_R) read_pos <= read_pos + NEXT_POS;
      if (instruction & FIFO_W) write_pos <= write_pos + NEXT_POS;
      if (instruction & FIFO_W) mem[write_pos] <= a_data;
    end
  end

  always_ff @(posedge clk) begin: generate_full_flag
    if (~nrst) begin
      full <= 'b0;
    end else begin
      case (instruction)
        FIFO_R: begin // only read === release full flag
          full <= 'b0;
        end
        FIFO_W: begin
          if (read_pos == write_pos + NEXT_POS) begin
            full <= 'b1;
          end
        end
        default: begin
          full <= full;
        end
      endcase
    end
  end

endmodule
