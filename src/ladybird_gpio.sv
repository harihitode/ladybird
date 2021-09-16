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

  // timer for interval
  logic [15:0]        r_timer;

  assign bus.gnt = 'b1;
  assign bus.data = (bus.data_gnt) ? data_out : 'z;
  assign data_in = bus.data;

  assign LED = led_reg;

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

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      led_reg <= '0;
      switch_reg <= SWITCH;
      button_reg <= BUTTON;
      r_timer <= '0;
    end else begin
      if (~nrst) begin
        led_reg <= '0;
        switch_reg <= SWITCH;
        button_reg <= BUTTON;
        r_timer <= '0;
      end else begin
        r_timer <= r_timer + 'd1;
        if (r_timer == I_READ_INTERVAL) begin
          if (|(switch_reg ^ SWITCH)) begin
            switch_reg <= SWITCH;
            switch_int <= 'b1;
          end else begin
            switch_int <= 'b0;
          end
          if (|(button_reg ^ BUTTON)) begin
            button_reg <= SWITCH;
            button_int <= 'b1;
          end else begin
            button_int <= 'b0;
          end
        end else begin
          button_int <= 'b0;
          switch_int <= 'b0;
        end
        if (led_write) begin
          led_reg <= data_in[3:0];
        end
      end
    end
  end

endmodule
