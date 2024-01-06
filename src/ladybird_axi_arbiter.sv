`timescale 1 ns / 1 ps

`include "../src/ladybird_axi.svh"

module ladybird_axi_arbiter
  (
   ladybird_axi_interface.slave i_axi_0,
   ladybird_axi_interface.slave i_axi_1,
   ladybird_axi_interface.master o_axi
   );

  // AW channel
  always_comb begin
    if (i_axi_0.awvalid) begin
      i_axi_0.awready = o_axi.awready;
      i_axi_1.awready = '0;
      o_axi.awvalid = i_axi_0.awvalid;
      o_axi.awaddr = i_axi_0.awaddr;
      o_axi.awprot = i_axi_0.awprot;
      o_axi.awid = i_axi_0.awid;
      o_axi.awlen = i_axi_0.awlen;
      o_axi.awsize = i_axi_0.awsize;
      o_axi.awburst = i_axi_0.awburst;
      o_axi.awlock = i_axi_0.awlock;
      o_axi.awcache = i_axi_0.awcache;
    end else begin
      i_axi_0.awready = '0;
      i_axi_1.awready = o_axi.awready;
      o_axi.awvalid = i_axi_1.awvalid;
      o_axi.awaddr = i_axi_1.awaddr;
      o_axi.awprot = i_axi_1.awprot;
      o_axi.awid = i_axi_1.awid;
      o_axi.awlen = i_axi_1.awlen;
      o_axi.awsize = i_axi_1.awsize;
      o_axi.awburst = i_axi_1.awburst;
      o_axi.awlock = i_axi_1.awlock;
      o_axi.awcache = i_axi_1.awcache;
    end
  end
  // W channel
  always_comb begin
    if (i_axi_0.wvalid) begin
      i_axi_0.wready = o_axi.wready;
      i_axi_1.wready = '0;
      o_axi.wvalid = i_axi_0.wvalid;
      o_axi.wdata = i_axi_0.wdata;
      o_axi.wstrb = i_axi_0.wstrb;
      o_axi.wid = i_axi_0.wid;
      o_axi.wlast = i_axi_0.wlast;
    end else begin
      i_axi_0.wready = '0;
      i_axi_1.wready = o_axi.wready;
      o_axi.wvalid = i_axi_1.wvalid;
      o_axi.wdata = i_axi_1.wdata;
      o_axi.wstrb = i_axi_1.wstrb;
      o_axi.wid = i_axi_1.wid;
      o_axi.wlast = i_axi_1.wlast;
    end
  end
  // B channel
  always_comb begin
    if (o_axi.bid == '0) begin
      i_axi_0.bvalid = o_axi.bvalid;
      i_axi_0.bresp = o_axi.bresp;
      i_axi_0.bid = o_axi.bid;
      i_axi_1.bvalid = '0;
      i_axi_1.bresp = '0;
      i_axi_1.bid = '0;
      o_axi.bready = i_axi_0.bready;
    end else begin
      i_axi_0.bvalid = '0;
      i_axi_0.bresp = '0;
      i_axi_0.bid = '0;
      i_axi_1.bvalid = o_axi.bvalid;
      i_axi_1.bresp = o_axi.bresp;
      i_axi_1.bid = o_axi.bid;
      o_axi.bready = i_axi_1.bready;
    end
  end
  // AR channel
  always_comb begin
    if (i_axi_0.arvalid) begin
      i_axi_0.arready = o_axi.arready;
      i_axi_1.arready = '0;
      o_axi.arvalid = i_axi_0.arvalid;
      o_axi.araddr = i_axi_0.araddr;
      o_axi.arprot = i_axi_0.arprot;
      o_axi.arid = i_axi_0.arid;
      o_axi.arlen = i_axi_0.arlen;
      o_axi.arsize = i_axi_0.arsize;
      o_axi.arburst = i_axi_0.arburst;
      o_axi.arlock = i_axi_0.arlock;
      o_axi.arcache = i_axi_0.arcache;
    end else begin
      i_axi_0.arready = '0;
      i_axi_1.arready = o_axi.arready;
      o_axi.arvalid = i_axi_1.arvalid;
      o_axi.araddr = i_axi_1.araddr;
      o_axi.arprot = i_axi_1.arprot;
      o_axi.arid = i_axi_1.arid;
      o_axi.arlen = i_axi_1.arlen;
      o_axi.arsize = i_axi_1.arsize;
      o_axi.arburst = i_axi_1.arburst;
      o_axi.arlock = i_axi_1.arlock;
      o_axi.arcache = i_axi_1.arcache;
    end
  end
  // R channel
  always_comb begin
    if (o_axi.rid == '0) begin
      i_axi_0.rvalid = o_axi.rvalid;
      i_axi_0.rdata = o_axi.rdata;
      i_axi_0.rresp = o_axi.rresp;
      i_axi_0.rid = o_axi.rid;
      i_axi_0.rlast = o_axi.rlast;
      i_axi_1.rvalid = '0;
      i_axi_1.rdata = '0;
      i_axi_1.rresp = '0;
      i_axi_1.rid = '0;
      i_axi_1.rlast = '0;
      o_axi.rready = i_axi_0.rready;
    end else begin
      i_axi_0.rvalid = '0;
      i_axi_0.rdata = '0;
      i_axi_0.rresp = '0;
      i_axi_0.rid = '0;
      i_axi_0.rlast = '0;
      i_axi_1.rvalid = o_axi.rvalid;
      i_axi_1.rdata = o_axi.rdata;
      i_axi_1.rresp = o_axi.rresp;
      i_axi_1.rid = o_axi.rid;
      i_axi_1.rlast = o_axi.rlast;
      o_axi.rready = i_axi_1.rready;
    end
  end
endmodule
