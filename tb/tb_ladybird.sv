`timescale 1 ns / 1 ps

module tb_ladybird;
  import ladybird_config::*;
  logic clk = '0;
  logic anrst = '0;
  logic anrst_c = '0;
  logic nrst = 'b1;
  logic uart_txd_in;
  logic uart_rxd_out;
  logic [3:0] btn;
  logic [3:0] sw = '0;
  logic [3:0] led;
  wire [1:0]  led_r;
  wire [1:0]  led_g;
  wire [1:0]  led_b;

  always_ff @(posedge clk) begin
    nrst <= anrst;
  end

  typedef enum logic [1:0] {
                            IDLE,
                            COMMAND,
                            DATA
                            } qspi_state_t;
  qspi_state_t qspi_state;
  logic [31:0] qspi_counter, qspi_command;
  wire         qspi_cs;
  wire [3:0]   qspi_dq;
  logic [7:0]  qspi_data;
  assign qspi_dq[0] = 1'bz;
  assign qspi_dq[1] = qspi_data[7];
  assign qspi_dq[2] = 1'bz;
  assign qspi_dq[3] = 1'bz;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      qspi_counter <= '0;
      qspi_state <= IDLE;
      qspi_command <= '0;
      qspi_data <= '0;
    end else begin
      if ((qspi_state == IDLE) && ~qspi_cs) begin
        qspi_state <= COMMAND;
        qspi_counter <= '0;
      end else if ((qspi_state == COMMAND) && (qspi_counter == 'd31)) begin
        qspi_state <= DATA;
        qspi_counter <= '0;
      end else if ((qspi_state == DATA) && (qspi_counter == 'd6)) begin
        qspi_state <= IDLE;
      end else begin
        qspi_counter <= qspi_counter + 'd1;
      end
      if (((qspi_state == IDLE) && ~qspi_cs) || (qspi_state == COMMAND)) begin
        qspi_command <= {qspi_command[30:0], qspi_dq[0]};
      end
      if ((qspi_state == COMMAND) && (qspi_counter == 'd31)) begin
        qspi_data <= 8'h61;
      end else begin
        qspi_data <= {qspi_data[6:0], 1'b0};
      end
    end
  end

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
    @(negedge clk);
    host_uart_bus.req = 'b1;
    host_uart_bus.wstrb = '1;
    @(posedge clk);
    host_uart_bus.req = '0;
    #10 anrst_c = 'b1;
    forever begin
      @(negedge clk);
      host_uart_bus.req = 'b1;
      host_uart_bus.wstrb = '0;
      @(posedge clk);
      host_uart_bus.req = '0;
      wait (host_uart_bus.data_gnt == 'b1);
      @(posedge clk);
      $display($time, " %d (%08x) (%c)", host_uart_bus.data, host_uart_bus.data, host_uart_bus.data);
      $finish;
    end
  end

  initial begin
    forever begin
      btn = 4'b0000;
      #1000;
      btn = 4'b0001;
      #200;
    end
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
