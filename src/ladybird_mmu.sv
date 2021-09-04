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

  assign bus.data = (i_ready & (|i_wstrb)) ? i_data : 'z;
  assign bus.req = i_valid & i_ready;
  assign bus.addr = i_addr;
  assign bus.wstrb = i_wstrb;
  assign o_valid = bus.data_gnt;
  assign o_data = bus.data;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      i_ready <= 'b1;
    end else begin
      if (~nrst) begin
        i_ready <= 'b1;
      end else begin
        if ((i_ready & i_valid) && (i_wstrb == '0)) begin
          i_ready <= 'b0;
        end else if (o_valid && o_ready) begin
          i_ready <= 'b1;
        end
      end
    end
  end

endmodule
