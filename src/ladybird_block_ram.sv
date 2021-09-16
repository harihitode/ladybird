`timescale 1 ns / 1 ps

module ladybird_block_ram
  import ladybird_config::*;
  (
   input logic clk,
   ladybird_bus.secondary bus,
   input logic nrst,
   input logic anrst
   );
  // 2 read latency
  localparam   READ_LATENCY = 2;
  logic [READ_LATENCY-1:0] req_l;
  logic [XLEN-1:0]         data_out;
  logic [XLEN-1:0]         data_in;

  assign bus.gnt = 'b1;
  assign bus.data_gnt = req_l[0];
  assign bus.data = (bus.data_gnt) ? data_out : 'z;
  assign data_in = bus.data;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      req_l <= '0;
    end else begin
      if (~nrst) begin
        req_l <= '0;
      end else begin
        req_l <= {bus.req & ~|bus.wstrb, req_l[$high(req_l):1]};
      end
    end
  end
  blk_mem_gen_0_spm BRAM_INST
    (
     .addra(bus.addr[9+2:2]),
     .clka(clk),
     .dina(data_in),
     .douta(data_out),
     .ena(bus.req),
     .wea(bus.wstrb)
     );

endmodule
