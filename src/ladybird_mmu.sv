`timescale 1 ns / 1 ps

module ladybird_mmu
  import ladybird_config::*;
  #(parameter SIMULATIUON = 0)
  (
   input logic              clk,
   input logic              i_valid,
   output logic             i_ready,
   input logic [XLEN-1:0]   i_addr,
   input logic [XLEN-1:0]   i_data,
   input logic [XLEN/8-1:0] i_wstrb,
   output logic             o_valid,
   input logic              o_ready,
   output logic [XLEN-1:0]  o_data,
   interface.primary        bus,
   input logic              anrst,
   input logic              nrst
   );

  typedef struct packed {
    logic              valid;
    logic              we;
    logic [2:0]        funct;
    logic [XLEN-1:0]   addr;
    logic [XLEN-1:0]   data;
  } request_t;

  request_t            request_buffer;
  logic                request_done;

  assign i_ready = ~request_buffer.valid;
  assign bus.data = (bus.req & request_buffer.we) ? request_buffer.data : 'z;
  assign bus.req = ~request_done & request_buffer.valid;
  assign bus.addr = request_buffer.addr;
  assign bus.wstrb = request_buffer.we ? 4'b0001 : 4'b0000;

  assign o_valid = bus.data_gnt;
  assign o_data = bus.data;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      request_buffer <= '0;
      request_done <= '0;
    end else begin
      if (~nrst) begin
        request_buffer <= '0;
        request_done <= '0;
      end else begin
        if (request_buffer.valid & bus.gnt) begin
          request_done <= '1;
        end else if (o_ready & o_valid) begin
          request_done <= '0;
        end
        if (i_ready & i_valid) begin
          request_buffer.valid <= i_ready & i_valid;
          request_buffer.addr <= i_addr;
          request_buffer.data <= i_data;
          request_buffer.funct <= 3'b001;
          if (i_wstrb == '0) begin
            request_buffer.we <= '0;
          end else begin
            request_buffer.we <= '1;
          end
        end else if (o_valid & o_ready) begin
          request_buffer <= '0;
        end
      end
    end
  end

endmodule
