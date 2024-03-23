`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_axi.svh"

module ladybird_cache
  import ladybird_config::*;
  #(
    parameter  LINE_W = 7,
    parameter  INDEX_W = 9,
    parameter  AXI_ID = 0,
    parameter  AXI_DATA_W = 32,
    localparam TAG_W = XLEN - (LINE_W - $clog2(8)) - INDEX_W
    )
  (
   input logic                  clk,
   input logic                  i_valid,
   input logic [XLEN-1:0]       i_addr,
   input logic [XLEN-1:0]       i_data,
   input logic [XLEN/8-1:0]     i_wen,
   input logic                  i_uncache,
   input logic                  i_flush,
   input logic                  i_invalidate,
   output logic                 i_ready,
   output logic                 o_valid,
   output logic [XLEN-1:0]      o_addr,
   output logic [2**LINE_W-1:0] o_data,
   ladybird_axi_interface.master axi,
   input logic                  nrst
   );

  localparam [XLEN-1:0]         AXI_DATA_ALIGNED_ADDR_MASK = ~((AXI_DATA_W / 8) - 1);
  localparam [XLEN-1:0]         NEXT_LINE = 1 << (LINE_W - $clog2(8));

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
    logic                       uncache;
    logic                       flush;
    logic                       invalidate;
    logic                       miss_with_writeback;
    logic [XLEN-1:0]            addr;
    logic [XLEN-1:0]            data;
    logic [XLEN/8-1:0]          wen;
    logic [ladybird_axi::LEN_W-1:0] burst_count;
    logic [ladybird_axi::LEN_W-1:0] burst_len;
    logic [ladybird_axi::SIZE_W-1:0] burst_size;
  } request_t;

  typedef enum                  logic [2:0] {
                                             IDLE,
                                             AW_CHANNEL,
                                             W_CHANNEL,
                                             B_CHANNEL,
                                             AR_CHANNEL,
                                             R_CHANNEL,
                                             REFRESH,
                                             INVALIDATE
                                             } state_t;

  addr_t request_addr;
  line_t line [2**INDEX_W];
  state_t state_d, state_q;
  request_t request_q;
  logic [2**LINE_W-1:0]         wr_line, rd_line, refresh_line;
  logic [(2**LINE_W)/8-1:0]     wr_line_en;
  logic                         cache_hit;
  logic                         miss_with_writeback;
  logic                         miss_without_writeback;
  logic                         o_valid_d;
  logic [XLEN-1:0]              o_addr_d;
  logic [2**LINE_W-1:0]         o_data_d;

  assign i_ready = (state_q == IDLE) ? '1 : '0;

  always_comb begin
    automatic line_t current_line = '0;
    if (request_q.valid) begin
      request_addr = request_q.addr;
    end else begin
      request_addr = i_addr;
    end
    current_line = line[request_addr.index];
    if (i_uncache || ~i_valid || (current_line.valid && current_line.tag == request_addr.tag)) begin
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
    if (((i_valid && ~i_uncache && ~i_flush && ~i_invalidate) ||
         (request_q.valid && ~request_q.uncache && ~request_q.flush && ~request_q.invalidate)) &&
        (current_line.valid && current_line.tag == request_addr.tag)) begin
      cache_hit = '1;
    end else begin
      cache_hit = '0;
    end
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
      blockaddr = {{{XLEN-$bits(waddr.blockaddr)}{1'b0}}, (waddr.blockaddr)} & ('1 << $clog2(XLEN/8));
      current_line = line[waddr.index];
      if (i >= blockaddr && i < blockaddr + XLEN/8) begin
        if (wen[i - blockaddr]) begin
          wr_line[i*8+:8] = wdata[(i - blockaddr)*8+:8];
          wr_line_en[i] = '1;
        end else begin
          wr_line[i*8+:8] = current_line.data[i*8+:8];
          wr_line_en[i] = '0;
        end
      end else begin
        wr_line[i*8+:8] = rd_line[i*8+:8];
        wr_line_en[i] = '0;
      end
    end
  end endgenerate

  always_comb begin
    if (state_q == IDLE) begin
      o_data_d = rd_line;
      o_valid_d = cache_hit;
      o_addr_d = request_addr;
    end else if (state_q == REFRESH) begin
      if (request_q.uncache) begin
        o_data_d = refresh_line;
        o_valid_d = '1;
        o_addr_d = request_addr;
      end else begin
        o_data_d = rd_line;
        o_valid_d = cache_hit;
        o_addr_d = request_addr;
      end
    end else begin
      o_data_d = '0;
      o_valid_d = '0;
      o_addr_d = '0;
    end
  end

  // output ctrl
  always_ff @(posedge clk) begin
    if (~nrst) begin
      o_data <= '0;
      o_valid <= '0;
      o_addr <= '0;
    end else begin
      o_data <= o_data_d;
      o_valid <= o_valid_d;
      o_addr <= o_addr_d;
    end
  end

  always_comb begin
    state_d = state_q;
    if (state_q == IDLE) begin
      if (i_valid & i_ready) begin
        if (miss_with_writeback) begin
          state_d = AW_CHANNEL;
        end else if (miss_without_writeback) begin
          state_d = AR_CHANNEL;
        end else if (i_uncache && |i_wen) begin
          state_d = AW_CHANNEL; // uncache write
        end else if (i_uncache) begin
          state_d = AR_CHANNEL; // uncache read
        end else if (i_flush) begin
          state_d = AW_CHANNEL;
        end
      end
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
        if (request_q.flush) begin
          if (request_addr.index == '1) begin
            state_d = REFRESH;
          end else begin
            state_d = AW_CHANNEL;
          end
        end else if (request_q.uncache) begin
          state_d = REFRESH;
        end else begin
          state_d = AR_CHANNEL;
        end
      end
    end else if (state_q == AR_CHANNEL) begin
      if (axi.arready & axi.arvalid) begin
        state_d = R_CHANNEL;
      end
    end else if (state_q == R_CHANNEL) begin
      if (axi.rvalid & axi.rlast & axi.rready) begin
        state_d = REFRESH;
      end
    end else if (state_q == REFRESH) begin
      state_d = IDLE;
    end else if (state_q == INVALIDATE) begin
      if (request_addr.index == '1) begin
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
        end else if (i_valid & i_ready) begin
          request_q.valid <= '1;
          request_q.uncache <= i_uncache;
          request_q.flush <= i_flush;
          request_q.invalidate <= i_invalidate;
          request_q.miss_with_writeback <= miss_with_writeback;
          if (i_flush || i_invalidate) begin
            request_q.addr <= '0;
          end else begin
            request_q.addr <= i_addr;
          end
          request_q.data <= i_data;
          request_q.wen <= i_wen;
          case (AXI_DATA_W)
            32: request_q.burst_size <= ladybird_axi::axi_burst_size_32;
            64: request_q.burst_size <= ladybird_axi::axi_burst_size_64;
            128: request_q.burst_size <= ladybird_axi::axi_burst_size_128;
            256: request_q.burst_size <= ladybird_axi::axi_burst_size_256;
            default: request_q.burst_size <= ladybird_axi::axi_burst_size_32;
          endcase
          request_q.burst_len <= 2**(LINE_W - $clog2(AXI_DATA_W)) - 1;
        end
      end else if (state_q == W_CHANNEL) begin
        if (axi.wvalid & axi.wready) begin
          request_q.burst_count <= request_q.burst_count + 'd1;
        end
      end else if (state_q == B_CHANNEL) begin
        if (axi.bvalid & axi.bready & request_q.flush) begin
          request_q.addr <= request_q.addr + NEXT_LINE;
        end
        request_q.burst_count <= 'd0;
      end else if (state_q == R_CHANNEL) begin
        if (axi.rvalid & axi.rready) begin
          request_q.burst_count <= request_q.burst_count + 'd1;
        end
      end else if (state_q == REFRESH) begin
        request_q <= '0;
      end else if (state_q == INVALIDATE) begin
        request_q.addr <= request_q.addr + NEXT_LINE;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      line <= '{default:'0};
      refresh_line <= '0;
    end else begin
      if (state_q == IDLE && cache_hit && |i_wen && ~i_uncache && ~i_flush && ~i_invalidate) begin
        line[request_addr.index].dirty <= '1;
        line[request_addr.index].data <= wr_line;
      end else if (state_q == B_CHANNEL && axi.bvalid && axi.bready && ~request_q.uncache) begin
        line[request_addr.index].dirty <= '0;
      end else if (state_q == R_CHANNEL && axi.rvalid && axi.rready) begin
        line[request_addr.index].valid <= '1;
        line[request_addr.index].dirty <= '0;
        line[request_addr.index].tag <= request_addr.tag;
        line[request_addr.index].data <= axi.rdata;
      end else if (state_q == REFRESH && cache_hit && |request_q.wen && ~request_q.uncache) begin
        line[request_addr.index].dirty <= '1;
        line[request_addr.index].data <= wr_line;
      end else if (state_q == INVALIDATE && request_q.invalidate) begin
        line[request_addr.index] <= '0;
      end
      if (state_q == R_CHANNEL && axi.rvalid && axi.rready) begin
        refresh_line <= axi.rdata;
      end
    end
  end

  // Instruction AW channel
  assign axi.awid = AXI_ID;
  assign axi.awaddr = (request_q.flush || request_q.miss_with_writeback) ?
                      {line[request_addr.index].tag, request_addr.index, {{LINE_W-$clog2(8)}{'0}}} :
                      request_q.addr & AXI_DATA_ALIGNED_ADDR_MASK;
  assign axi.awlen = request_q.burst_len;
  assign axi.awsize = request_q.burst_size;
  assign axi.awburst = ladybird_axi::axi_incrementing_burst;
  assign axi.awlock = '0;
  assign axi.awcache = '0;
  assign axi.awprot = '0;
  assign axi.awvalid = (state_q == AW_CHANNEL) ? '1 : '0;
  // Instruction W channel
  assign axi.wid = AXI_ID;
  assign axi.wstrb = (request_q.flush || request_q.miss_with_writeback) ? '1 : wr_line_en;
  assign axi.wdata = (request_q.flush || request_q.miss_with_writeback) ? line[request_addr.index].data : wr_line;
  assign axi.wlast = (request_q.burst_count == request_q.burst_len) ? '1 : '0;
  assign axi.wvalid = (state_q == W_CHANNEL) ? '1 : '0;
  // Instruction B channel
  assign axi.bready = (state_q == B_CHANNEL) ? '1 : '0;
  // Instruction AR channel
  assign axi.arid = AXI_ID;
  assign axi.araddr = request_q.addr & AXI_DATA_ALIGNED_ADDR_MASK;
  assign axi.arlen = request_q.burst_len;
  assign axi.arsize = request_q.burst_size;
  assign axi.arburst = ladybird_axi::axi_incrementing_burst;
  assign axi.arlock = '0;
  assign axi.arcache = '0;
  assign axi.arprot = '0;
  assign axi.arvalid = (state_q == AR_CHANNEL) ? '1 : '0;
  // Instruction R channel
  assign axi.rready = (state_q == R_CHANNEL) ? '1 : '0;

endmodule
