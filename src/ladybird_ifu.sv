`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_axi.svh"
`include "../src/ladybird_riscv_helper.svh"

module ladybird_ifu
  import ladybird_config::*;
  #(parameter AXI_ID = 'd0,
    parameter AXI_DATA_W = 32)
  (
   input logic             clk,
   input logic [XLEN-1:0]  pc,
   input logic             pc_valid,
   output logic            pc_ready,
   input logic             flush,
   output logic [XLEN-1:0] inst,
   output logic            inst_valid,
   input logic             inst_ready,
   output logic [XLEN-1:0] inst_pc,
   ladybird_axi_interface.master i_axi,
   input logic             nrst
   );

  localparam               CACHE_LINE_W = $clog2(AXI_DATA_W);
  localparam               CACHE_INDEX_W = L1I_CACHE_INDEX_W;

  struct                   packed {
    logic                  valid;
    logic [XLEN-1:0]       pc;
  } request_q, request_d;

  logic                    cache_req, cache_ready;
  logic [XLEN-1:0]         cache_addr;
  logic                    line_valid, line_ready;
  // verilator lint_off: UNUSED
  // TODO: C-Extension
  logic [XLEN-1:0]         line_addr_d, line_addr_q;
  logic [2**CACHE_LINE_W/XLEN-1:0][XLEN-1:0] line_data_d, line_data_q;
  // verilator lint_on: UNUSED

  always_comb begin
    cache_req = pc_valid;
    cache_addr = pc;
    pc_ready = (~request_q.valid || (request_q.pc == line_addr_d && line_valid)) && cache_ready && inst_ready;
    if (request_q.valid && ~flush) begin
      if (request_q.pc == line_addr_d) begin
        inst_valid = line_valid;
      end else begin
        inst_valid = '0;
      end
    end else begin
      inst_valid = '0;
    end
    inst = line_data_d[line_addr_d[CACHE_LINE_W-1:$clog2(XLEN/8)]];
    inst_pc = line_addr_d;
  end

  assign line_ready = inst_ready;

  always_comb begin
    if ((pc_valid && pc_ready) || flush) begin
      request_d.pc = pc;
      request_d.valid = '1;
    end else if (inst_valid & inst_ready) begin
      request_d = '0;
    end else begin
      request_d = request_q;
    end
  end

  ladybird_cache #(.LINE_W(CACHE_LINE_W), .INDEX_W(CACHE_INDEX_W), .AXI_ID(AXI_ID), .AXI_DATA_W(AXI_DATA_W))
  ICACHE
    (
     .clk(clk),
     .i_valid(cache_req),
     .i_addr(cache_addr),
     .i_data('0),
     .i_wen('0),
     .i_ready(cache_ready),
     .i_uncache('0),
     .i_flush('0),
     .i_invalidate('0),
     .o_valid(line_valid),
     .o_addr(line_addr_d),
     .o_data(line_data_d),
     .o_ready(line_ready),
     .axi(i_axi),
     .nrst(nrst)
     );

  always_ff @(posedge clk) begin
    if (~nrst) begin
      request_q <= '0;
      line_addr_q <= '0;
      line_data_q <= '0;
    end else begin
      request_q <= request_d;
      if (line_valid) begin
        line_addr_q <= line_addr_d;
        line_data_q <= line_data_d;
      end
    end
  end

endmodule
