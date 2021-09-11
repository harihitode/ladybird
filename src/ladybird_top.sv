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

  // access type
  typedef enum        logic [1:0] {
                                   DISTRIBUTED_RAM,
                                   BLOCK_RAM,
                                   DYNAMIC_RAM,
                                   UART
                                   } access_t;

  function automatic access_t access_type(input logic [XLEN-1:0] addr);
    case (addr[31:27])
      4'hF:    return UART;
      4'h8:    return DISTRIBUTED_RAM;
      4'h9:    return BLOCK_RAM;
      default: return DYNAMIC_RAM;
    endcase
  endfunction
  access_t                    dbus_access;
  access_t                    ibus_access;
  assign dbus_access = access_type(d_bus.addr);
  assign ibus_access = access_type(i_bus.addr);

  // internal bus
  ladybird_bus uart_bus();
  ladybird_bus block_ram_bus();
  ladybird_bus distributed_ram_bus();
  // ladybird_bus dynamic_ram_bus();

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

  logic              start;

  always_ff @(posedge clk_i, negedge anrst_i) begin
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
     .start_pc(32'h9000_0000),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  // scratch_pad_memory SPM
  //   (
  //    .clka(clk_i),
  //    .ena(re | we),
  //    .wea(wstrb_l),
  //    .addra(addr_l[11:2]),
  //    .dina(data_l),
  //    .douta(spm_rd_data)
  //    );

  ladybird_serial_interface #(.I_BYTES(1), .O_BYTES(1), .WTIME(16'h364))
  SERIAL_IF
    (
     .clk(clk_i),
     .uart_txd_in(uart_txd_in),
     .uart_rxd_out(uart_rxd_out_i),
     .bus(d_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

  ladybird_ram #(.ADDR_W(4))
  IRAM
    (
     .clk(clk_i),
     .bus(i_bus),
     .nrst(nrst),
     .anrst(anrst_i)
     );

endmodule
