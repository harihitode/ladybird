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
  ladybird_bus core_bus[2]();

  //////////////////////////////////////////////////////////////////////
  logic                       start; // start 1 cycle to wakeup core
  logic                       switch_int; // switch interrupt
  logic                       button_int; // button interrupt
  //////////////////////////////////////////////////////////////////////

  // internal bus
  ladybird_bus peripheral_bus[5]();

  IBUF clock_buf (.I(clk), .O(clk_i));
  IBUF txd_in_buf (.I(uart_txd_in), .O(uart_txd_in_i));
  OBUF rxd_out_buf (.I(uart_rxd_out_i), .O(uart_rxd_out));
  IBUF anrst_buf (.I(anrst), .O(anrst_i));

  generate for (genvar i = 0; i < 4; i++) begin: implementation_check_io_buf
    IBUF btn_in_buf (.I(btn[i]), .O(btn_i[i]));
    OBUF led_out_buf (.I(led_i[i]), .O(led[i]));
  end endgenerate

  always_ff @(posedge clk_i) begin: synchronous_reset
    nrst <= anrst_i & ~(switch_int | button_int); // any_interrupt
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

  ladybird_core #(.SIMULATION(SIMULATION))
  DUT
    (
     .clk(clk_i),
     .i_bus(core_bus[I_BUS]),
     .d_bus(core_bus[D_BUS]),
     .start(start),
     .start_pc(32'h9000_0000),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_serial_interface #(.I_BYTES(1), .O_BYTES(1), .WTIME(16'h364))
  SERIAL_IF
    (
     .clk(clk_i),
     .uart_txd_in(uart_txd_in),
     .uart_rxd_out(uart_rxd_out_i),
     .bus(peripheral_bus[UART]),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_block_ram SPM_RAM_INST
    (
     .clk(clk_i),
     .bus(peripheral_bus[BRAM]),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_inst_ram #(.DISTRIBUTED_RAM(0))
  INST_RAM_INST
    (
     .clk(clk_i),
     .bus(peripheral_bus[IRAM]),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  // current implementation is distributed ram
  ladybird_ram #(.ADDR_W(4))
  DYNAMIC_RAM_INST
    (
     .clk(clk_i),
     .bus(peripheral_bus[DRAM]),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_gpio GPIO_INST
    (
     .clk(clk_i),
     .bus(peripheral_bus[GPIO]),
     .SWITCH('0),
     .BUTTON(btn_i),
     .LED(led_i),
     .switch_int(switch_int),
     .button_int(button_int),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_crossbar #(.N_PERIPHERAL_BUS(5))
  CROSS_BAR
    (
     .clk(clk_i),
     .core_ports(core_bus),
     .peripheral_ports(peripheral_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

endmodule
