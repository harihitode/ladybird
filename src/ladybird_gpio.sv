`timescale 1 ns / 1 ps
`default_nettype none

// LED, Switch, Button...
module ladybird_gpio
  #(
    localparam                    BYTE_W = 8,
    parameter                     N_INPUT = 2,
    parameter                     N_OUTPUT = 1,
    parameter                     I_BLOCKING_MODE = 0,
    parameter logic [N_INPUT-1:0] I_TOGGLE_MASK = 0,
    parameter                     GPIO_I_DEFAULT_VALUE = 0
    )
  (
   input wire                              clk,
   ladybird_bus.secondary bus,
   input wire [N_INPUT-1:0][BYTE_W-1:0]    GPIO_I,
   output logic [N_OUTPUT-1:0][BYTE_W-1:0] GPIO_O,
   output logic [N_INPUT-1:0]              pending,
   input wire [N_INPUT-1:0]                complete,
   input wire                              nrst
   );
  localparam                               I_WIDTH = BYTE_W * N_INPUT;
  localparam                               O_WIDTH = BYTE_W * N_OUTPUT;
  localparam                               GPIO_ADDR_W = $clog2(N_INPUT+N_OUTPUT+1);

  logic                                    gpio_write;
  logic                                    gpio_read;
  logic [GPIO_ADDR_W-1:0]                  gpio_read_addr;
  logic [N_INPUT-1:0][BYTE_W-1:0]          gpio_i_reg;
  logic [N_OUTPUT-1:0][BYTE_W-1:0]         gpio_o_reg;

  logic [31:0]                             data_out;
  logic [31:0]                             data_in;

  logic [N_INPUT+N_OUTPUT-1:0][BYTE_W-1:0] gpio_reg;
  assign gpio_reg = {gpio_i_reg, gpio_o_reg};

  always_ff @(posedge clk) begin
    if (~nrst) begin
      gpio_read <= '0;
      gpio_read_addr <= '0;
    end else begin
      if (bus.req & ~(|bus.wstrb)) begin
        gpio_read <= 'b1;
        gpio_read_addr <= bus.addr[GPIO_ADDR_W-1:0];
      end else if (bus.data_gnt) begin
        gpio_read <= '0;
      end
    end
  end

  always_comb begin: GPIO_data_out
    if (gpio_read) begin
      if (I_BLOCKING_MODE && (gpio_read_addr >= N_OUTPUT)) begin
        bus.data_gnt = pending[gpio_read_addr - N_OUTPUT];
      end else begin
        bus.data_gnt = 1'b1;
      end
      if (gpio_read_addr < (N_INPUT + N_OUTPUT)) begin
        data_out = {4{gpio_reg[gpio_read_addr]}};
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
    if ((bus.req & (|bus.wstrb)) && (bus.addr[GPIO_ADDR_W-1:0] < N_OUTPUT)) begin
      gpio_write = 'b1;
    end else begin
      gpio_write = 'b0;
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      gpio_o_reg <= '0;
    end else begin
      if (gpio_write) begin
        gpio_o_reg <= data_in[BYTE_W-1:0];
      end
    end
  end

  generate for (genvar i = 0; i < N_INPUT; i++) begin: GPIO_i_reg
    always_ff @(posedge clk) begin
      if (~nrst) begin
        gpio_i_reg[i] <= '0;
        pending[i] <= '0;
      end else begin
        if (pending[i]) begin
          if (I_BLOCKING_MODE) begin
            pending[i] <= 'b0;
          end else begin
            if (complete[i]) begin
              pending[i] <= '0;
            end
          end
        end else begin
          if (gpio_i_reg[i] != GPIO_I[i]) begin
            gpio_i_reg[i] <= GPIO_I[i];
            if (I_TOGGLE_MASK) begin
              if (GPIO_I[i] != GPIO_I_DEFAULT_VALUE) begin
                pending[i] <= 'b1;
              end
            end else begin
              pending[i] <= 'b1;
            end
          end
        end
      end
    end
  end endgenerate

endmodule

`default_nettype wire
