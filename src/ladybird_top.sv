`timescale 1 ns / 1 ps

module ladybird_top
  import ladybird_config::*;
  #(parameter SIMULATION = 0)
  (
   input logic        clk,
   input logic        uart_txd_in,
   output logic       uart_rxd_out,
   ladybird_bus       inst_bus,
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
  logic [7:0]         data;
  logic               receive;
  logic               nrst;

  ladybird_bus data_bus();

  IBUF clock_buf (.I(clk), .O(clk_i));
  IBUF txd_in_buf (.I(uart_txd_in), .O(uart_txd_in_i));
  OBUF rxd_out_buf (.I(uart_rxd_out_i), .O(uart_rxd_out));
  IBUF anrst_buf (.I(anrst), .O(anrst_i));
  generate for (genvar i = 0; i < 4; i++) begin
    IBUF btn_in_buf (.I(btn[i]), .O(btn_i[i]));
    OBUF led_out_buf (.I(led_i[i]), .O(led[i]));
  end endgenerate

  always_ff @(posedge clk_i, negedge anrst_i) begin
    if (~anrst_i) begin
      led_i <= '0;
    end else begin
      if (~nrst) begin
        led_i <= '0;
      end else begin
        led_i <= btn_i;
      end
    end
  end

  always_ff @(posedge clk_i) begin
    nrst <= anrst_i;
  end

  logic [XLEN-1:0] addr;
  logic            re;
  logic            we;
  logic            addr_valid;
  logic            addr_ready;

  logic [XLEN-1:0] wr_data;
  logic            wr_data_valid;
  logic            wr_data_ready;

  logic [XLEN-1:0] rd_data;
  logic            rd_data_valid;
  logic            rd_data_ready;

  logic [XLEN-1:0] register [32];

  logic            uart_pop, uart_push;

  always_comb begin
    if (addr_valid && addr == {XLEN{1'b1}}) begin
      if (re) begin
        uart_pop = 'b1;
      end else begin
        uart_pop = 'b0;
      end
      if (we) begin
        uart_push = 'b1;
      end else begin
        uart_push = 'b0;
      end
    end else begin
      uart_pop = 'b0;
      uart_push = 'b0;
    end
  end

  ladybird_core DUT
    (
     .clk(clk_i),
     .inst(inst_bus),
     .data(data_bus),
     .nrst(nrst),
     .anrst(anrst)
     );

  ladybird_serial_interface #(.I_BYTES(1), .O_BYTES(1), .WTIME(16'h364))
  SERIAL_IF
    (
     .clk(clk_i),
     .uart_txd_in(uart_txd_in),
     .uart_rxd_out(uart_rxd_out_i),
     .i_data(rd_data[7:0]),
     .i_valid(uart_pop),
     .i_ready(),
     .o_data(wr_data[7:0]),
     .o_valid(uart_push),
     .o_ready('b1),
     .nrst(nrst),
     .anrst(anrst_i)
     );

endmodule
