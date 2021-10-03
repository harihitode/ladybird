`timescale 1 ns / 1 ps

module ladybird_top
  import ladybird_config::*;
  #(parameter SIMULATION = 0)
  (
   input logic        clk,
   input logic        uart_txd_in,
   output logic       uart_rxd_out,
   input logic [3:0]  btn,
   input logic [3:0]  sw,
   output logic [3:0] led,
   input logic        anrst
   );

  logic               clk_i;
  logic               uart_txd_in_i;
  logic               uart_rxd_out_i;
  logic               anrst_i;
  logic [3:0]         btn_i;
  logic [3:0]         sw_i;
  logic [3:0]         led_i;
  //
  logic               nrst;

  // from core 2 bus data/instruction
  ladybird_bus core_bus[2]();

  localparam logic [XLEN-1:0] START_PC = 32'h9000_0000;
  localparam logic [XLEN-1:0] TVEC_PC = 32'h9000_0010;
  //////////////////////////////////////////////////////////////////////
  logic                       start; // start 1 cycle to wakeup core
  logic [7:0]                 pending; // interrupt pending
  logic                       complete; // interrupt complete
  logic [7:0]                 complete_i;
  //////////////////////////////////////////////////////////////////////

  // internal bus
  ladybird_bus peripheral_bus[5]();

  IBUF clock_buf (.I(clk), .O(clk_i));
  IBUF txd_in_buf (.I(uart_txd_in), .O(uart_txd_in_i));
  OBUF rxd_out_buf (.I(uart_rxd_out_i), .O(uart_rxd_out));
  IBUF anrst_buf (.I(anrst), .O(anrst_i));

  generate for (genvar i = 0; i < 4; i++) begin: implementation_check_io_buf
    IBUF btn_in_buf (.I(btn[i]), .O(btn_i[i]));
    IBUF sw_in_buf (.I(sw[i]), .O(sw_i[i]));
    OBUF led_out_buf (.I(led_i[i]), .O(led[i]));
  end endgenerate

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

  ladybird_core #(.SIMULATION(SIMULATION),
                  .TVEC(TVEC_PC))
  CORE
    (
     .clk(clk_i),
     .i_bus(core_bus[I_BUS]),
     .d_bus(core_bus[D_BUS]),
     .start(start),
     .start_pc(START_PC),
     .pending(|pending),
     .complete(complete),
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

  ladybird_block_ram #(.ADDR_W(11), .READ_LATENCY(2))
  SPM_RAM_INST
    (
     .clk(clk_i),
     .bus(peripheral_bus[BRAM]),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_inst_ram #(.DISTRIBUTED_RAM(SIMULATION))
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

  always_comb begin
    complete_i = {8{complete}};
  end
  ladybird_gpio #(.E_WIDTH(4), .N_INPUT(2), .N_OUTPUT(1))
  GPIO_INST
    (
     .clk(clk_i),
     .bus(peripheral_bus[GPIO]),
     .GPIO_I({sw_i, btn_i}),
     .GPIO_O(led_i),
     .pending(pending),
     .complete(complete_i),
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
