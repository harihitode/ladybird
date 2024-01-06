`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_axi.svh"
`include "../src/ladybird_riscv_helper.svh"

module ladybird_ifu
  import ladybird_config::*;
  #(parameter AXI_ID_I = 'd0)
  (
   input logic             clk,
   input logic [XLEN-1:0]  pc,
   input logic             pc_valid,
   output logic            pc_ready,
   output logic [XLEN-1:0] inst,
   output logic            inst_valid,
   input logic             inst_ready,
   output logic [XLEN-1:0] inst_pc,
   ladybird_axi_interface.master i_axi,
   input logic             nrst
   );

  localparam [XLEN-1:0]    i_address_mask = ~((i_axi.AXI_DATA_W / 8) - 1);

  typedef enum             logic [1:0] {
                                        IDLE,
                                        AR_CHANNEL,
                                        R_CHANNEL
                                        } state_t;

  typedef struct           packed {
    logic                  valid;
    logic [2:0]            funct;
    logic [XLEN-1:0]       addr;
  } i_request_t;

  typedef struct           packed {
    logic                  valid;
    logic [XLEN-1:0]       addr;
    logic [XLEN-1:0]       data;
  } rddata_buf_t;

  state_t i_state_q, i_state_d;
  i_request_t i_request_q, i_request_d;
  rddata_buf_t i_buf_d, i_buf_q;
  logic [XLEN-1:0]         i_rdata;

  always_comb begin
    automatic logic read_start;
    read_start = '0;
    i_request_d = i_request_q;
    i_state_d = i_state_q;
    if (pc_valid & pc_ready) begin
      read_start = '1;
      i_request_d.valid = '1;
      i_request_d.funct = ladybird_riscv_helper::FUNCT3_LW;
      i_request_d.addr = pc;
    end
    if (i_state_q == IDLE) begin
      if (read_start) begin
        i_state_d = AR_CHANNEL;
      end
    end else if (i_state_q == AR_CHANNEL) begin
      if (i_axi.arready & i_axi.arvalid) begin
        i_state_d = R_CHANNEL;
      end
    end else if (i_state_q == R_CHANNEL) begin
      if (i_axi.rvalid & i_axi.rlast & i_axi.rready) begin
        i_state_d = IDLE;
      end
    end
  end

  assign pc_ready = (i_state_q == IDLE) ? '1 : '0;

  always_comb begin
    i_buf_d = i_buf_q;
    if (i_buf_q.valid && inst_ready) begin
      i_buf_d.valid = '0;
    end else if (i_axi.rvalid && i_axi.rready) begin
      i_buf_d.valid = '1;
      i_buf_d.data = i_rdata;
      i_buf_d.addr = i_request_q.addr;
    end
  end

  assign inst_valid = i_buf_q.valid;
  assign inst = i_buf_q.data;
  assign inst_pc = i_buf_q.addr;

  always_ff @(posedge clk) begin
    if (~nrst) begin
      i_request_q <= '0;
      i_state_q <= IDLE;
      i_buf_q <= '0;
    end else begin
      i_request_q <= i_request_d;
      i_state_q <= i_state_d;
      i_buf_q <= i_buf_d;
    end
  end

  // Instruction AW channel
  assign i_axi.awid = AXI_ID_I;
  assign i_axi.awaddr = i_request_q.addr & i_address_mask;
  assign i_axi.awlen = '0;
  assign i_axi.awsize = ladybird_axi::axi_burst_size_32;
  assign i_axi.awburst = ladybird_axi::axi_incrementing_burst;
  assign i_axi.awlock = '0;
  assign i_axi.awcache = '0;
  assign i_axi.awprot = '0;
  assign i_axi.awvalid = '0;
  // Instruction W channel
  assign i_axi.wid = AXI_ID_I;
  assign i_axi.wstrb = '0;
  assign i_axi.wdata = '0;
  assign i_axi.wlast = '1;
  assign i_axi.wvalid = '0;
  // Instruction B channel
  assign i_axi.bready = '0;
  // Instruction AR channel
  assign i_axi.arid = AXI_ID_I;
  assign i_axi.araddr = i_request_q.addr & i_address_mask;
  assign i_axi.arlen = '0;
  assign i_axi.arsize = ladybird_axi::axi_burst_size_32;
  assign i_axi.arburst = ladybird_axi::axi_incrementing_burst;
  assign i_axi.arlock = '0;
  assign i_axi.arcache = '0;
  assign i_axi.arprot = '0;
  assign i_axi.arvalid = (i_state_q == AR_CHANNEL) ? '1 : '0;
  // Instruction R channel
  assign i_axi.rready = ~i_buf_q.valid;
  always_comb begin
    i_rdata = '0;
    if (i_request_q.funct == ladybird_riscv_helper::FUNCT3_LHU) begin
      if (i_request_q.addr[1] == 1'b0) begin
        i_rdata = {{16{1'b0}}, i_axi.rdata[15:0]};
      end else begin
        i_rdata = {{16{1'b0}}, i_axi.rdata[31:16]};
      end
    end else begin
      i_rdata = i_axi.rdata;
    end
  end

endmodule
