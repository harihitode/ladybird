`timescale 1 ns / 1 ps

module ladybird_ram
  import ladybird_config::*;
  #(
    parameter DATA_W = 32,
    parameter ADDR_W = 10
    )
  (
   input logic clk,
   ladybird_bus.secondary bus,
   input logic nrst
   );

  logic [DATA_W-1:0] ram [2**ADDR_W];
  logic [DATA_W-1:0] data_out;
  logic [DATA_W-1:0] data_in;
  logic              data_gnt;

  assign bus.gnt = 'b1;
  assign bus.data = (bus.data_gnt) ? data_out : 'z;
  assign bus.data_gnt = data_gnt;
  assign data_in = bus.data;

  always_ff @(posedge clk) begin
    if (~nrst) begin
      data_gnt <= '0;
      data_out <= '0;
      ram <= '{default:'0};
    end else begin
      if (bus.req & (|bus.wstrb)) begin
        data_gnt <= 'b0;
        for (int i = 0; i < 4; i++) begin
          if (bus.wstrb[i]) ram[bus.addr[ADDR_W+2-1:2]][8*i+:8] <= data_in[8*i+:8];
        end
      end else if (bus.req) begin
        data_gnt <= 'b1;
        data_out <= ram[bus.addr[ADDR_W+2-1:2]];
      end else begin
        data_gnt <= 'b0;
      end
    end
  end

endmodule
