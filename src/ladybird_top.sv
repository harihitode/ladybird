`timescale 1 ns / 1 ps

module ladybird_top
  import ladybird_config::*;
  #(parameter SIMULATION = 0)
  (
   input logic        clk,
   input logic        uart_txd_in,
   output logic       uart_rxd_out,
   input logic [3:0]  btn,
   output logic [3:0] led,
   input logic        anrst
   );

  logic               clk_i;
  logic               uart_txd_in_i;
  logic               uart_rxd_out_i;
  logic               anrst_i;
  logic [3:0]         btn_i;
  logic [3:0]         led_i;
  //
  logic               nrst;

  // from core 2 bus data/instruction
  ladybird_bus i_bus();
  ladybird_bus d_bus();

  //////////////////////////////////////////////////////////////////////
  logic                       start; // start 1 cycle to wakeup core
  //////////////////////////////////////////////////////////////////////

  // internal bus
  ladybird_bus uart_bus();
  ladybird_bus distributed_ram_bus();
  ladybird_bus block_ram_bus();
  ladybird_bus dynamic_ram_bus();

  IBUF clock_buf (.I(clk), .O(clk_i));
  IBUF txd_in_buf (.I(uart_txd_in), .O(uart_txd_in_i));
  OBUF rxd_out_buf (.I(uart_rxd_out_i), .O(uart_rxd_out));
  IBUF anrst_buf (.I(anrst), .O(anrst_i));

  generate for (genvar i = 0; i < 4; i++) begin: implementation_check_io_buf
    IBUF btn_in_buf (.I(btn[i]), .O(btn_i[i]));
    OBUF led_out_buf (.I(led_i[i]), .O(led[i]));
  end endgenerate

  always_ff @(posedge clk_i, negedge anrst_i) begin
    if (~anrst_i) begin
      led_i <= '0;
    end else begin
      // push each button to turn on each led
      if (~nrst) begin
        led_i <= '0;
      end else begin
        led_i <= btn_i;
      end
    end
  end

  always_ff @(posedge clk_i) begin: synchronous_reset
    nrst <= anrst_i;
  end

  always_ff @(posedge clk_i, negedge anrst_i) begin: waking_core_up
    if (~anrst_i) begin
      start <= '0;
    end else begin
      if (~nrst) begin
        start <= '0;
      end else begin
        start <= 'b1;
      end
    end
  end

  ladybird_core DUT
    (
     .clk(clk_i),
     .i_bus(i_bus),
     .d_bus(d_bus),
     .start(start),
     .start_pc(32'h8000_0000),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_serial_interface #(.I_BYTES(1), .O_BYTES(1), .WTIME(16'h364))
  SERIAL_IF
    (
     .clk(clk_i),
     .uart_txd_in(uart_txd_in),
     .uart_rxd_out(uart_rxd_out_i),
     .bus(uart_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_ram #(.ADDR_W(4))
  BLOCK_RAM_MOCK
    (
     .clk(clk_i),
     .bus(block_ram_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_ram #(.ADDR_W(4))
  DISTRIBUTED_RAM
    (
     .clk(clk_i),
     .bus(distributed_ram_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_ram #(.ADDR_W(4))
  DYNAMIC_RAM_MOCK
    (
     .clk(clk_i),
     .bus(dynamic_ram_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_crossbar CROSS_BAR
    (
     .clk(clk_i),
     .core_ports('{d_bus, i_bus}),
     .peripheral_ports('{distributed_ram_bus, block_ram_bus, dynamic_ram_bus, uart_bus}),
     .nrst(nrst),
     .anrst(anrst)
     );

endmodule
