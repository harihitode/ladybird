`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_axi.svh"
`include "../src/ladybird_riscv_helper.svh"

module ladybird_ifu
  import ladybird_config::*;
  #(parameter AXI_ID_I = 'd0,
    parameter AXI_DATA_W = 32)
  (
   input logic             clk,
   input logic [XLEN-1:0]  pc,
   input logic             pc_valid,
   output logic            pc_ready,
   output logic [XLEN-1:0] inst,
   output logic            inst_valid,
   // verilator lint_off: UNUSED
   input logic             inst_ready,
   // verilator lint_on: UNUSED
   output logic [XLEN-1:0] inst_pc,
                           ladybird_axi_interface.master i_axi,
   input logic             nrst
   );

  localparam               CACHE_LINE_W = $clog2(AXI_DATA_W);
  localparam               CACHE_INDEX_W = 9;

  logic                    cache_req, cache_ready;
  logic [XLEN-1:0]         cache_addr;
  logic                    line_valid;
  // verilator lint_off: UNUSED
  logic [XLEN-1:0]         line_addr_d, line_addr_q;
  logic [2**CACHE_LINE_W/XLEN-1:0][XLEN-1:0] line_data_d, line_data_q;
  // verilator lint_on: UNUSED

  always_comb begin
    cache_req = pc_valid;
    cache_addr = pc;
    pc_ready = cache_ready;
    inst_valid = line_valid;
    inst = line_data_d[line_addr_d[CACHE_LINE_W-1:$clog2(XLEN/8)]];
    inst_pc = line_addr_d;
  end

  ladybird_cache #(.LINE_W(CACHE_LINE_W), .INDEX_W(CACHE_INDEX_W), .AXI_ID(AXI_ID_I))
  ICACHE
    (
     .clk(clk),
     .i_valid(cache_req),
     .i_addr(cache_addr),
     .i_data('0),
     .i_wen('0),
     .i_ready(cache_ready),
     .o_valid(line_valid),
     .o_addr(line_addr_d),
     .o_data(line_data_d),
     .axi(i_axi),
     .nrst(nrst)
     );

  always_ff @(posedge clk) begin
    if (~nrst) begin
      line_addr_q <= '0;
      line_data_q <= '0;
    end else begin
      if (line_valid) begin
        line_addr_q <= line_addr_d;
        line_data_q <= line_data_d;
      end
    end
  end

endmodule
