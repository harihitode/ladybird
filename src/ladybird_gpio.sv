`timescale 1 ns / 1 ps

// LED, Switch, Button...
module ladybird_gpio
  #(
    parameter logic [15:0] I_READ_INTERVAL = 16'h364
    )
  (
   input logic        clk,
   ladybird_bus.secondary bus,
   input logic [3:0]  SWITCH,
   input logic [3:0]  BUTTON,
   output logic [3:0] LED,
   output logic       switch_int,
   output logic       button_int,
   input logic        accept,
   input logic        nrst,
   input logic        anrst
   );
  // ADDRESS MAP
  // 0x0000_0000: switch
  // 0x0000_0004: button
  // 0x0000_0008: led
  logic               led_write;
  logic [3:0]         led_reg;
  logic [3:0]         switch_reg;
  logic [3:0]         button_reg;

  logic [31:0]        data_out;
  logic [31:0]        data_in;

  // internal buf
  logic [3:0]         SWITCH_I;
  logic [3:0]         BUTTON_I;

  assign bus.gnt = 'b1;
  assign bus.data = (bus.data_gnt) ? data_out : 'z;
  assign data_in = bus.data;

  assign LED = led_reg | button_reg | switch_reg;

  always_comb begin
    if ((bus.req & (|bus.wstrb)) && (bus.addr[3:0] == 4'h8)) begin
      led_write = 'b1;
    end else begin
      led_write = 'b0;
    end
  end

  always_comb begin
    if (bus.req & ~(|bus.wstrb)) begin
      bus.data_gnt = 'b1;
      if (bus.addr[3:0] == 4'h0) begin
        data_out = {28'd0, switch_reg};
      end else if (bus.addr[3:0] == 4'h4) begin
        data_out = {28'd0, button_reg};
      end else if (bus.addr[3:0] == 4'h8) begin
        data_out = {28'd0, led_reg};
      end else begin
        data_out = '0;
      end
    end else begin
      bus.data_gnt = 'b0;
      data_out = '0;
    end
  end

  assign switch_int = |switch_reg;
  assign button_int = |button_reg;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      led_reg <= '0;
      switch_reg <= '0;
      button_reg <= '0;
      SWITCH_I <= '0;
      BUTTON_I <= '0;
    end else begin
      if (~nrst) begin
        led_reg <= '0;
        switch_reg <= '0;
        button_reg <= '0;
        SWITCH_I <= '0;
        BUTTON_I <= '0;
      end else begin
        SWITCH_I <= SWITCH;
        BUTTON_I <= BUTTON;
        if (switch_int & accept) begin
          switch_reg <= '0;
        end else begin
          switch_reg <= switch_reg | SWITCH_I;
        end
        if (button_int & accept) begin
          button_reg <= '0;
        end else begin
          button_reg <= button_reg | BUTTON_I;
        end
        if (led_write) begin
          led_reg <= data_in[3:0];
        end
      end
    end
  end

endmodule
