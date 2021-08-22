interface ladybird_bus
  import ladybird_config::*;
  ();
  logic req, gnt;
  logic [XLEN-1:0] addr;
  wire [XLEN-1:0]  data;
  logic [XLEN/8-1:0] wstrb;
  modport primary (output req, addr, wstrb, input gnt, inout data);
  modport secondary (input req, addr, wstrb, output gnt, inout data);
endinterface
