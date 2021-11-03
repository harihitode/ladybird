`timescale 1 ns / 1 ps

module ladybird_block_ram
  import ladybird_config::*;
  #(parameter ADDR_W = 11,
    parameter READ_LATENCY = 2)
  (
   input logic clk,
   ladybird_bus.secondary bus,
   input logic nrst
   );
  logic [READ_LATENCY-1:0] req_l;
  logic [XLEN-1:0]         data_out;
  logic [XLEN-1:0]         data_in;
  logic [3:0]              wstrb;

  assign bus.gnt = 'b1;
  assign bus.data_gnt = req_l[0];
  assign bus.data = (bus.data_gnt) ? {data_out[7:0], data_out[15:8], data_out[23:16], data_out[31:24]} : 'z;
  assign data_in = {bus.data[7:0], bus.data[15:8], bus.data[23:16], bus.data[31:24]};
  assign wstrb = (bus.req & bus.gnt) ? {bus.wstrb[0], bus.wstrb[1], bus.wstrb[2], bus.wstrb[3]} : '0;

  always_ff @(posedge clk) begin
    if (~nrst) begin
      req_l <= '0;
    end else begin
      req_l <= {bus.req & ~|bus.wstrb, req_l[$high(req_l):1]};
    end
  end
  blk_mem_gen_0_spm BRAM_INST
    (
     .addra(bus.addr[ADDR_W+2-1:2]),
     .clka(clk),
     .dina(data_in),
     .douta(data_out),
     .ena(1'b1),
     .wea(wstrb)
     );

endmodule
