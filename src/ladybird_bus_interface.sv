interface ladybird_bus
  import ladybird_config::*;
  ();
  logic req, gnt;
  logic [XLEN-1:0] addr;
  wire [XLEN-1:0]  data;
  logic [XLEN/8-1:0] wstrb;
  logic              data_gnt;
  modport primary (output req, addr, wstrb, input gnt, inout data, input data_gnt);
  modport secondary (input req, addr, wstrb, output gnt, inout data, output data_gnt);
endinterface
