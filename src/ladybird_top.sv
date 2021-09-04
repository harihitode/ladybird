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

  ladybird_bus data_bus();
  ladybird_bus inst_bus();

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

  logic [XLEN-1:0] addr, addr_l, data_l, rd_data, wr_data;
  logic            re, re_l;
  logic            we, we_l;

  logic [7:0]      uart_push_data;
  logic            uart_ready;
  logic            uart_push;

  logic [7:0]      uart_pop_data;
  logic            uart_valid;
  logic            uart_pop;

  assign data_bus.gnt = ~re_l & ~we_l;
  assign data_bus.data_gnt = uart_pop & uart_valid;
  assign data_bus.data = (uart_pop & uart_valid) ? rd_data : 'z;
  assign addr = data_bus.addr;
  assign re = data_bus.req & ~(|data_bus.wstrb);
  assign we = data_bus.req & (|data_bus.wstrb);
  assign uart_push_data = data_bus.data[7:0];
  assign rd_data = {24'd0, uart_pop_data};
  assign wr_data = data_bus.data;

  assign uart_pop = ((addr_l == '1) && re_l) ? 'b1 : 'b0;
  assign uart_push = ((addr_l == '1) && we_l) ? 'b1 : 'b0;

  always_ff @(posedge clk_i, negedge anrst_i) begin
    if (~anrst_i) begin
      addr_l <= '0;
      re_l <= '0;
      we_l <= '0;
    end else begin
      if (~nrst) begin
        addr_l <= '0;
        re_l <= '0;
        we_l <= '0;
      end else begin
        if (re_l) begin
          if (uart_pop & uart_valid) begin
            re_l <= '0;
          end
        end else begin
          addr_l <= addr;
          re_l <= re;
        end
        if (we_l) begin
          if (uart_push & uart_ready) begin
            we_l <= '0;
          end
        end else begin
          addr_l <= addr;
          data_l <= wr_data;
          we_l <= we;
        end
      end
    end
  end

  ladybird_core DUT
    (
     .clk(clk_i),
     .inst(inst_bus),
     .data(data_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_serial_interface #(.I_BYTES(1), .O_BYTES(1), .WTIME(16'h364))
  SERIAL_IF
    (
     .clk(clk_i),
     .uart_txd_in(uart_txd_in),
     .uart_rxd_out(uart_rxd_out_i),
     .i_data(uart_push_data),
     .i_valid(uart_push),
     .i_ready(uart_ready),
     .o_data(uart_pop_data),
     .o_valid(uart_valid),
     .o_ready(uart_pop),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_ram #(.ADDR_W(3))
  IRAM
    (
     .clk(clk_i),
     .bus(inst_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

endmodule
