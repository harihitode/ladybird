`timescale 1 ns / 1 ps
module tb_ladybird_serial;
  import ladybird_config::*;
  logic clk = '0;
  initial forever #5 clk = ~clk;
  logic anrst = '0;
  logic nrst = '0;

  logic uart_txd_in;
  logic uart_rxd_out;

  ladybird_bus bus_device();
  ladybird_bus bus_host();

  logic [31:0] read_data;
  assign bus_host.data = 32'h62;
  assign bus_device.data = 'z;
  assign read_data = bus_device.data;

  initial begin
    $display($time, " WAITING 500ns");
    #500;
    @(negedge clk);
    anrst = '1;
    nrst = '1;
    bus_host.req = '1;
    bus_host.addr = '1;
    bus_host.wstrb = '1;
    //
    bus_device.req = '1;
    bus_device.addr = '1;
    bus_device.wstrb = '0;
    @(posedge clk);
    wait(bus_host.req & bus_host.gnt);
    wait(bus_device.req & bus_device.gnt);
    bus_host.req = '0;
    $display($time, " REQUEST SUCCESS");
    bus_device.req = '0;
  end

  always_ff @(posedge clk) begin
    if (bus_device.data_gnt) begin
      $display($time, " RECEIVE %08x", read_data);
      $finish;
    end
  end

  ladybird_serial_interface DUT
    (
     .clk(clk),
     .uart_txd_in(uart_txd_in),
     .uart_rxd_out(uart_rxd_out),
     .bus(bus_device),
     .anrst(anrst),
     .nrst(nrst)
     );

  ladybird_serial_interface HOST
    (
     .clk(clk),
     .uart_txd_in(uart_rxd_out),
     .uart_rxd_out(uart_txd_in),
     .bus(bus_host),
     .anrst(anrst),
     .nrst(nrst)
     );

endmodule
