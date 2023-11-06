`ifndef LADYBIRD_AXI_SVH
`define LADYBIRD_AXI_SVH

// verilator lint_off UNUSEDPARAM
package ladybird_axi;
  localparam PROT_W = 3;
  localparam LEN_W = 8;
  localparam SIZE_W = 3;
  localparam BURST_W = 2;
  localparam LOCK_W = 2;
  localparam CACHE_W = 4;
  localparam RESP_W = 2;
  localparam logic [BURST_W-1:0] axi_fixed_burst = 'd0;
  localparam logic [BURST_W-1:0] axi_incrementing_burst = 'd1;
  localparam logic [BURST_W-1:0] axi_wrapping_burst = 'd2;
  localparam logic [SIZE_W-1:0]  axi_burst_size_32 = 3'b010;
endpackage
  // verilator lint_on UNUSEDPARAM

`endif
