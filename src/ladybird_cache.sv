`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_axi.svh"

module ladybird_cache
  import ladybird_config::*;
  #(
    parameter  LINE_W = 7,
    parameter  INDEX_W = 9,
    parameter  AXI_ID = 0,
    localparam TAG_W = XLEN - (LINE_W - $clog2(8)) - INDEX_W
    )
  (
   input logic                  clk,
   input logic                  i_valid,
   input logic [XLEN-1:0]       i_addr,
   input logic [XLEN-1:0]       i_data,
   input logic [XLEN/8-1:0]     i_wen,
   output logic                 i_ready,
   output logic                 o_valid,
   output logic [XLEN-1:0]      o_addr,
   output logic [2**LINE_W-1:0] o_data,
   ladybird_axi_interface.master axi,
   input logic                  nrst
   );

  localparam                    BURST_LEN = (2**LINE_W) / axi.AXI_DATA_W;
  localparam [XLEN-1:0]         i_address_mask = ~((axi.AXI_DATA_W / 8) - 1);

  typedef struct                packed {
    logic [TAG_W-1:0]           tag;
    logic [INDEX_W-1:0]         index;
    logic [LINE_W-$clog2(8)-1:0] blockaddr;
  } addr_t;

  typedef struct                packed {
    logic                       valid;
    logic                       dirty;
    logic [TAG_W-1:0]           tag;
    logic [2**LINE_W-1:0]       data;
  } line_t;

  typedef struct                packed {
    logic                       valid;
    logic [XLEN-1:0]            addr;
    logic [XLEN-1:0]            data;
    logic [XLEN/8-1:0]          wen;
  } request_t;

  typedef enum                  logic [2:0] {
                                             IDLE,
                                             AW_CHANNEL,
                                             W_CHANNEL,
                                             B_CHANNEL,
                                             AR_CHANNEL,
                                             R_CHANNEL
                                             } state_t;

  addr_t request_addr;
  line_t line [2**INDEX_W];
  state_t state_d, state_q;
  request_t request_q;
  logic [2**LINE_W-1:0]         wr_line, rd_line;
  logic                         cache_hit;
  logic                         miss_with_writeback;
  logic                         miss_without_writeback;

  assign i_ready = (state_q == IDLE) ? '1 : '0;

  always_comb begin
    automatic line_t current_line = '0;
    if (request_q.valid) begin
      request_addr = request_q.addr;
    end else begin
      request_addr = i_addr;
    end
    current_line = line[request_addr.index];
    if (~i_valid || (current_line.valid && current_line.tag == request_addr.tag)) begin
      miss_with_writeback = '0;
      miss_without_writeback = '0;
    end else begin
      if (current_line.dirty) begin
        miss_with_writeback = '1;
        miss_without_writeback = '0;
      end else begin
        miss_with_writeback = '0;
        miss_without_writeback = '1;
      end
    end
    cache_hit = ~(miss_with_writeback | miss_without_writeback) & i_valid & i_ready;
    rd_line = current_line.data;
  end

  generate for (genvar i = 0; i < 2**(LINE_W-$clog2(8)); i++) begin: BYTE_CTRL_CACHE_LINE
    always_comb begin
      // verilator lint_off UNUSED
      automatic addr_t             waddr = '0;
      automatic logic [XLEN-1:0]   wdata = '0;
      automatic logic [XLEN/8-1:0] wen = '0;
      automatic line_t             current_line = '0;
      automatic int blockaddr = 0;
      // verilator lint_on UNUSED
      if (request_q.valid) begin
        waddr = request_q.addr;
        wen = request_q.wen;
        wdata = request_q.data;
      end else begin
        waddr = i_addr;
        wen = i_wen;
        wdata = i_data;
      end
      blockaddr = {{{XLEN-$bits(waddr.blockaddr)}{1'b0}}, waddr.blockaddr};
      current_line = line[waddr.index];
      if (i >= blockaddr && i < blockaddr + XLEN/8) begin
        if (wen[i - blockaddr]) begin
          wr_line[i*8+:8] = wdata[(i - blockaddr)*8+:8];
        end else begin
          wr_line[i*8+:8] = current_line.data[i*8+:8];
        end
      end else begin
        wr_line[i*8+:8] = rd_line[i*8+:8];
      end
    end
  end endgenerate

  // output ctrl
  always_ff @(posedge clk) begin
    if (~nrst) begin
      o_data <= '0;
      o_valid <= '0;
      o_addr <= '0;
    end else begin
      o_data <= rd_line;
      o_valid <= cache_hit;
      o_addr <= request_addr;
    end
  end

  always_comb begin
    state_d = state_q;
    if (state_q == IDLE) begin
      if (i_valid & i_ready & miss_with_writeback) begin
        state_d = AW_CHANNEL;
      end else if (i_valid & i_ready & miss_without_writeback) begin
        state_d = AR_CHANNEL;
      end
    end else if (state_q == AW_CHANNEL) begin
      if (axi.awready & axi.awvalid) begin
        state_d = W_CHANNEL;
      end
    end else if (state_q == W_CHANNEL) begin
      if (axi.wready & axi.wlast & axi.wready) begin
        state_d = B_CHANNEL;
      end
    end else if (state_q == B_CHANNEL) begin
      if (axi.bready & axi.bvalid) begin
        state_d = AR_CHANNEL;
      end
    end else if (state_q == AR_CHANNEL) begin
      if (axi.arready & axi.arvalid) begin
        state_d = R_CHANNEL;
      end
    end else if (state_q == R_CHANNEL) begin
      if (axi.rvalid & axi.rlast & axi.rready) begin
        state_d = IDLE;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      state_q <= IDLE;
      request_q <= '0;
    end else begin
      state_q <= state_d;
      if (state_q == IDLE) begin
        if (cache_hit) begin
          request_q <= '0;
        end else begin
          request_q.valid <= '1;
          request_q.addr <= i_addr;
          request_q.data <= i_data;
          request_q.wen <= i_wen;
        end
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      line <= '{default:'0};
    end else begin
      if (state_q == IDLE && cache_hit && |i_wen) begin
        line[request_addr.index].dirty <= '1;
        line[request_addr.index].data <= wr_line;
      end else if (state_q == R_CHANNEL && axi.rvalid & axi.rready) begin
        line[request_addr.index].valid <= '1;
        line[request_addr.index].dirty <= '0;
        line[request_addr.index].tag <= request_addr.tag;
        line[request_addr.index].data <= axi.rdata;
      end
    end
  end

  // Instruction AW channel
  assign axi.awid = AXI_ID;
  assign axi.awaddr = request_q.addr & i_address_mask;
  assign axi.awlen = BURST_LEN - 1;
  assign axi.awsize = ladybird_axi::axi_burst_size_32;
  assign axi.awburst = ladybird_axi::axi_incrementing_burst;
  assign axi.awlock = '0;
  assign axi.awcache = '0;
  assign axi.awprot = '0;
  assign axi.awvalid = '0;
  // Instruction W channel
  assign axi.wid = AXI_ID;
  assign axi.wstrb = '0;
  assign axi.wdata = '0;
  assign axi.wlast = '1;
  assign axi.wvalid = '0;
  // Instruction B channel
  assign axi.bready = '0;
  // Instruction AR channel
  assign axi.arid = AXI_ID;
  assign axi.araddr = request_q.addr & i_address_mask;
  assign axi.arlen = BURST_LEN - 1;
  assign axi.arsize = ladybird_axi::axi_burst_size_32;
  assign axi.arburst = ladybird_axi::axi_incrementing_burst;
  assign axi.arlock = '0;
  assign axi.arcache = '0;
  assign axi.arprot = '0;
  assign axi.arvalid = (state_q == AR_CHANNEL) ? '1 : '0;
  // Instruction R channel
  assign axi.rready = '1;

endmodule
