`timescale 1 ns / 1 ps
module ladybird_bus_arbitrator_beh
  #(
    parameter N_INPUT = 2
    )
  (
   input logic  clk,
   output logic o_data,
   interface in_bus [N_INPUT],
   interface out_bus,
   input logic  nrst,
   input logic  anrst
   );

  typedef struct packed {
    logic        req;
    logic [3:0]  wstrb;
    logic [31:0] addr;
    logic [31:0] data;
  } bus_request_s;

  bus_request_s  bus_requests [N_INPUT];
  int            selected_bus;
  logic [31:0]   selected_data;
  int            current_bus = 0;
  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      current_bus <= '0;
    end else begin
      current_bus <= selected_bus;
    end
  end

  generate for (genvar i = 0; i < N_INPUT; i++) begin
    always_comb begin
      bus_requests[i].req = in_bus[i].req;
      bus_requests[i].wstrb = in_bus[i].wstrb;
      bus_requests[i].addr = in_bus[i].addr;
      bus_requests[i].data = (bus_requests[i].wstrb != '0) ? in_bus[i].data : 'z;
    end
  end endgenerate

  always_comb begin
    selected_bus = 0;
    // priority: 0 > 1 > 2 > 3 ...
    for (int i = 0; i < N_INPUT; i++) begin
      if (bus_requests[i].req) begin
        selected_bus = i;
        break;
      end
    end
    out_bus.req = bus_requests[selected_bus].req;
    out_bus.wstrb = bus_requests[selected_bus].wstrb;
    out_bus.addr = bus_requests[selected_bus].addr;
    selected_data = (bus_requests[selected_bus].req & (|bus_requests[selected_bus].wstrb)) ? bus_requests[selected_bus].data : 'z;
  end
  assign out_bus.data = selected_data;

  generate for (genvar i = 0; i < N_INPUT; i++) begin
    assign in_bus[i].secondary.data = (i == current_bus && in_bus[i].wstrb == '0) ? out_bus.data : 'z;
    assign in_bus[i].secondary.gnt = (i == current_bus) ? out_bus.gnt : '0;
    assign in_bus[i].secondary.data_gnt = (i == current_bus) ? out_bus.data_gnt : '0;
  end endgenerate

endmodule
