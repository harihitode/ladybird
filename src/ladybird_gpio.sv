`timescale 1 ns / 1 ps

// LED, Switch, Button...
module ladybird_gpio
  #(
    parameter  E_WIDTH = 4,
    parameter  N_INPUT = 2,
    parameter  N_OUTPUT = 1
    )
  (
   input logic                         clk,
   ladybird_bus.secondary bus,
   input logic [E_WIDTH*N_INPUT-1:0]   GPIO_I,
   output logic [E_WIDTH*N_OUTPUT-1:0] GPIO_O,
   output logic [E_WIDTH*N_INPUT-1:0]  pending,
   input logic [E_WIDTH*N_INPUT-1:0]   complete,
   input logic                         nrst
   );

  localparam                           I_WIDTH = E_WIDTH * N_INPUT;
  localparam                           O_WIDTH = E_WIDTH * N_OUTPUT;

  logic [E_WIDTH*N_INPUT-1:0]          complete_mask;

  logic                                gpio_write;
  logic [I_WIDTH-1:0]                  gpio_i_reg;
  logic [O_WIDTH-1:0]                  gpio_o_reg;

  logic [31:0]                         data_out;
  logic [31:0]                         data_in;

  // ADDRESS MAP
  // 0x0000_0000: switch
  // 0x0000_0004: button
  // 0x0000_0008: led
  always_comb begin: ADDRESS_MAP
    if (bus.req & ~(|bus.wstrb)) begin
      bus.data_gnt = 'b1;
      if (bus.addr[3:0] == 4'h0) begin
        data_out = {{(32-E_WIDTH){1'b0}}, gpio_i_reg[1*E_WIDTH-1-:E_WIDTH]};
      end else if (bus.addr[3:0] == 4'h4) begin
        data_out = {{(32-E_WIDTH){1'b0}}, gpio_i_reg[2*E_WIDTH-1-:E_WIDTH]};
      end else if (bus.addr[3:0] == 4'h8) begin
        data_out = {{(32-E_WIDTH){1'b0}}, gpio_o_reg[E_WIDTH-1:0]};
      end else begin
        data_out = '0;
      end
    end else begin
      bus.data_gnt = 'b0;
      data_out = '0;
    end
  end

  assign bus.gnt = 'b1;
  assign bus.data = (bus.data_gnt) ? data_out : 'z;
  assign data_in = bus.data;

  assign GPIO_O = gpio_o_reg;

  always_comb begin
    if ((bus.req & (|bus.wstrb)) && (bus.addr[3:0] == 4'h8)) begin
      gpio_write = 'b1;
    end else begin
      gpio_write = 'b0;
    end
  end

  assign complete_mask = pending & complete;
  always_ff @(posedge clk) begin
    if (~nrst) begin
      gpio_i_reg <= '0;
      pending <= '0;
      gpio_o_reg <= '0;
    end else begin
      gpio_i_reg <= ~complete_mask & (gpio_i_reg | GPIO_I);
      pending <= ~complete_mask & (gpio_i_reg ^ GPIO_I);
      if (gpio_write) begin
        gpio_o_reg <= data_in[E_WIDTH-1:0];
      end
    end
  end

endmodule
