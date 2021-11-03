`timescale 1 ns / 1 ps

module tb_ladybird;
  import ladybird_config::*;
  // clocking
  logic clk = '0;
  default clocking cb @(posedge clk);
  endclocking
  // reset
  logic anrst = '0;
  logic nrst = '0;
  always_ff @(posedge clk) begin
    nrst <= anrst;
  end
  // other signals
  logic uart_txd_in;
  logic uart_rxd_out;
  logic [3:0] btn;
  logic [3:0] sw;
  logic [3:0] led;
  wire [1:0]  led_r;
  wire [1:0]  led_g;
  wire [1:0]  led_b;
  wire [7:0]  ja, jb, jc, jd;
  logic [7:0] ja_i, jb_i, jc_i, jd_i;

  assign ja[7:4] = ja_i[7:4];
  assign jb[7:4] = jb_i[7:4];
  assign jc[7:4] = jc_i[7:4];
  assign jd[7:4] = jd_i[7:4];
  assign ja[3:0] = 'z;
  assign jb[3:0] = 'z;
  assign jc[3:0] = 'z;
  assign jd[3:0] = 'z;

  wire        qspi_cs;
  wire [3:0]  qspi_dq;
  logic [7:0] qspi_data = 8'h61;
  assign qspi_dq[0] = 1'bz;
  assign qspi_dq[1] = qspi_data[7];
  assign qspi_dq[2] = 1'bz;
  assign qspi_dq[3] = 1'bz;

  logic [31:0] iram [] = '{
                           ADDI(5'd1, 5'd0, 12'hfff),
                           LB(5'd2, 12'h000, 5'd1),
                           ADDI(5'd2, 5'd2, 12'h001),
                           SB(5'd2, 12'h000, 5'd1),
                           JAL(5'd0, -21'd12)
                           };
  ladybird_bus inst_ram_writer();
  ladybird_bus inst_bus();
  ladybird_bus core_ibus();
  ladybird_bus host_uart_bus();
  logic [31:0] iram_data;
  assign inst_ram_writer.data = iram_data;
  task write_instruction ();
    for (int i = 0; i < $size(iram); i++) begin
      @(negedge clk);
      inst_ram_writer.req = 'b1;
      inst_ram_writer.addr = (i << 2);
      inst_ram_writer.wstrb = '1;
      iram_data = iram[i];
      @(posedge clk);
      wait (inst_ram_writer.req && inst_ram_writer.gnt);
    end
    inst_ram_writer.req = 'b0;
    $display("instruction has been written.");
  endtask

  initial forever #5 clk = ~clk;

  assign host_uart_bus.data = (host_uart_bus.data_gnt) ? 'z : 32'h63;
  assign host_uart_bus.addr = '1;
  initial begin
    #100 anrst = 'b1;
    write_instruction();
    ##1;
    host_uart_bus.req = 'b1;
    host_uart_bus.wstrb = '1;
    ##1;
    host_uart_bus.req = '0;
    forever begin
      ##1;
      host_uart_bus.req = 'b1;
      host_uart_bus.wstrb = '0;
      ##1;
      host_uart_bus.req = '0;
      wait (host_uart_bus.data_gnt == 'b1);
      ##1;
      $display($time, " %d (%08x) (%c)", host_uart_bus.data, host_uart_bus.data, host_uart_bus.data);
      $finish;
    end
  end

  initial begin
    btn = '0;
    ja_i = '0;
    jb_i = '0;
    jc_i = '0;
    jd_i = '1;
    sw = '0;
    #500;
    btn = 'b1;
    #500;
    btn = 'b0;
    #500;
    jd_i = 8'b10100000;
    #500;
    jd_i = '1;
  end

  ladybird_top #(.SIMULATION(1))
  DUT (
       .*
       );

  ladybird_bus_arbitrator_beh #(.N_INPUT(2))
  IBUS_ARB
    (
     .clk(clk),
     .in_bus('{inst_ram_writer, core_ibus}),
     .out_bus(inst_bus),
     .nrst(nrst),
     .anrst(anrst)
     );

  ladybird_ram_beh #(.DATA_W(XLEN), .ADDR_W(3))
  IRAM
    (
     .clk(clk),
     .bus(inst_bus),
     .nrst(nrst),
     .anrst(anrst)
     );

  ladybird_serial_interface #(.I_BYTES(1), .O_BYTES(1), .WTIME(16'h364))
  SERIAL_IF
    (
     .clk(clk),
     .uart_txd_in(uart_rxd_out),
     .uart_rxd_out(uart_txd_in),
     .bus(host_uart_bus),
     .nrst(nrst)
     );

endmodule
