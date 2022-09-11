`timescale 1 ns / 1 ps
`default_nettype none

module ladybird_top
  import ladybird_config::*;
  #(parameter SIMULATION = 0)
  (
   input wire         clk,
   // UART Serial
   input wire         uart_txd_in,
   output logic       uart_rxd_out,
   // GPIOs
   input wire [3:0]   btn,
   input wire [3:0]   sw,
   output logic [3:0] led,
   output logic [1:0] led_r,
   output logic [1:0] led_g,
   output logic [1:0] led_b,
   // QSPI to FLASH RAM
   output logic       qspi_cs,
   inout wire [3:0]   qspi_dq,
   input wire         anrst
   );

  // Sync. Reset (negative)
  logic               nrst;
  logic [3:0]         cled;

  // from core 2 bus data/instruction
  ladybird_bus_interface core_bus[2]();

  localparam logic [XLEN-1:0] START_PC = 32'h9000_0000;
  localparam logic [XLEN-1:0] TVEC_PC = 32'h9000_0010;
  //////////////////////////////////////////////////////////////////////
  logic                       start; // start 1 cycle to wakeup core
  logic [7:0]                 pending; // interrupt pending
  logic                       complete; // interrupt complete
  logic [7:0]                 complete_i;
  //////////////////////////////////////////////////////////////////////

  // internal bus
  ladybird_bus_interface peripheral_bus[NUM_PERIPHERAL]();

  assign led_r = {2{cled[2] & cled[3]}};
  assign led_g = {2{cled[1] & cled[3]}};
  assign led_b = {2{cled[0] & cled[3]}};

  always_ff @(posedge clk) begin: synchronous_reset
    nrst <= anrst;
  end

  always_ff @(posedge clk) begin: waking_core_up
    if (~nrst) begin
      start <= '0;
    end else begin
      start <= 'b1;
    end
  end

  ladybird_core #(.SIMULATION(SIMULATION),
                  .TVEC(TVEC_PC))
  CORE
    (
     .clk(clk),
     .i_bus(core_bus[I_BUS]),
     .d_bus(core_bus[D_BUS]),
     .start(start),
     .start_pc(START_PC),
     .pending(|pending),
     .complete(complete),
     .nrst(nrst)
     );

  ladybird_serial_interface #(.I_BYTES(1), .O_BYTES(1), .WTIME(16'h364))
  SERIAL_IF
    (
     .clk(clk),
     .uart_txd_in(uart_txd_in),
     .uart_rxd_out(uart_rxd_out),
     .bus(peripheral_bus[UART]),
     .nrst(nrst)
     );

  ladybird_block_ram #(.ADDR_W(11), .READ_LATENCY(2))
  SPM_RAM_INST
    (
     .clk(clk),
     .bus(peripheral_bus[BRAM]),
     .nrst(nrst)
     );

  ladybird_inst_ram #(.DISTRIBUTED_RAM(1))
  INST_RAM_INST
    (
     .clk(clk),
     .bus(peripheral_bus[IRAM]),
     .nrst(nrst)
     );

  // current implementation is distributed ram
  ladybird_ram #(.ADDR_W(4))
  DYNAMIC_RAM_INST
    (
     .clk(clk),
     .bus(peripheral_bus[DRAM]),
     .nrst(nrst)
     );

  always_comb begin
    complete_i = {8{complete}};
  end
  ladybird_gpio #(.E_WIDTH(4), .N_INPUT(2), .N_OUTPUT(2))
  GPIO_INST
    (
     .clk(clk),
     .bus(peripheral_bus[GPIO]),
     .GPIO_I({sw, btn}),
     .GPIO_O({cled, led}),
     .pending(pending),
     .complete(complete_i),
     .nrst(nrst)
     );

  ladybird_qspi_interface QSPI_IF
    (
     .sck(clk),
     .cs(qspi_cs),
     .dq(qspi_dq),
     .bus(peripheral_bus[QSPI]),
     .nrst(nrst)
     );

  ladybird_crossbar #(.N_PERIPHERAL_BUS(NUM_PERIPHERAL))
  CROSS_BAR
    (
     .clk(clk),
     .core_ports(core_bus),
     .peripheral_ports(peripheral_bus),
     .nrst(nrst)
     );

`ifndef SIMULATION
  STARTUPE2 QSPI_SCLK
    (
     .CFGCLK(),
     .CFGMCLK(),
     .EOS(),
     .PREQ(),
     .CLK(1'b0),
     .GSR(1'b0),
     .GTS(1'b0),
     .KEYCLEARB(1'b0),
     .PACK(1'b0),
     .USRCCLKO(clk), // clk to QSPI's SCLK
     .USRCCLKTS(1'b0),
     .USRDONEO(1'b0),
     .USRDONETS(1'b0)
     );
`endif

endmodule

`default_nettype wire
