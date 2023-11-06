`include "../src/ladybird_axi.svh"

// verilator lint_off UNUSEDSIGNAL
interface ladybird_axi_interface
#(
  parameter AXI_ADDR_W = 32,
  parameter AXI_DATA_W = 32,
  parameter AXI_ID_W = 4)
(
  input logic aclk
);
  import ladybird_axi::*;
  // write address channel
  logic [AXI_ID_W-1:0]     awid;
  logic [AXI_ADDR_W-1:0]   awaddr;
  logic [LEN_W-1:0]        awlen;
  logic [SIZE_W-1:0]       awsize;
  logic [BURST_W-1:0]      awburst;
  logic [LOCK_W-1:0]       awlock;
  logic [CACHE_W-1:0]      awcache;
  logic [PROT_W:0]         awprot;
  logic                    awvalid;
  logic                    awready;
  // write data channel
  logic [AXI_ID_W-1:0]     wid;
  logic [AXI_DATA_W-1:0]   wdata;
  logic [AXI_DATA_W/8-1:0] wstrb;
  logic                    wlast;
  logic                    wvalid;
  logic                    wready;
  // write response channel
  logic [AXI_ID_W-1:0]     bid;
  logic [RESP_W-1:0]       bresp;
  logic                    bvalid;
  logic                    bready;
  // read address channel
  logic [AXI_ID_W-1:0]     arid;
  logic [AXI_ADDR_W-1:0]   araddr;
  logic [LEN_W-1:0]        arlen;
  logic [SIZE_W-1:0]       arsize;
  logic [BURST_W-1:0]      arburst;
  logic [LOCK_W-1:0]       arlock;
  logic [CACHE_W-1:0]      arcache;
  logic [PROT_W-1:0]       arprot;
  logic                    arvalid;
  logic                    arready;
  // read data channel
  logic                    rready;
  logic [AXI_DATA_W-1:0]   rdata;
  logic [AXI_ID_W-1:0]     rid;
  logic                    rlast;
  logic [RESP_W-1:0]       rresp;
  logic                    rvalid;

`ifdef LADYBIRD_SIMULATION_AXI_HELPER
  task read_1beat(input logic [AXI_ADDR_W-1:0] addr, output logic [AXI_DATA_W-1:0] data);
    @(posedge aclk);
    araddr = addr;
    arvalid = '1;
    arid = '0;
    arlen = '0;
    arsize = $clog2(AXI_DATA_W / 8);
    wait (arvalid & arready);
    @(posedge aclk);
    arvalid = '0;
    rready = '1;
    wait (rvalid & rready);
    data = rdata;
  endtask

  task write_1beat(input logic [AXI_ADDR_W-1:0] addr, input logic [AXI_DATA_W-1:0] data);
    @(posedge aclk);
    awaddr = addr;
    awvalid = '1;
    awid = '0;
    awlen = '0;
    awsize = $clog2(AXI_DATA_W / 8);
    wait (awvalid & awready);
    @(posedge aclk);
    awvalid = '0;
    wvalid = '1;
    wlast = '1;
    wdata = data;
    wstrb = '1;
    wait (wvalid & wready);
    @(posedge aclk);
    wvalid = '0;
    wlast = '0;
    wstrb = '0;
    bready = '1;
    wait (bvalid & bready);
    @(posedge aclk);
    bready = '0;
  endtask
`endif

  modport master (
                  output awvalid, awaddr, awprot, wvalid, wdata, wstrb, bready, arvalid, araddr, arprot, rready,
                  output awid, awlen, awsize, awburst, awlock, awcache, wid, wlast, arid, arlen, arsize, arburst, arlock, arcache,
                  input  awready, wready, bvalid, bresp, arready, rvalid, rdata, rresp,
                  input  bid, rid, rlast);

  modport slave (
                 input  awvalid, awaddr, awprot, wvalid, wdata, wstrb, bready, arvalid, araddr, arprot, rready,
                 input  awid, awlen, awsize, awburst, awlock, awcache, wid, wlast, arid, arlen, arsize, arburst, arlock, arcache,
                 output awready, wready, bvalid, bresp, arready, rvalid, rdata, rresp,
                 output bid, rid, rlast);
endinterface
// verilator lint_on UNUSEDSIGNAL
