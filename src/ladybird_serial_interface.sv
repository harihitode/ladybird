`timescale 1 ns / 1 ps

module ladybird_serial_interface
  // 100MHz 115200bps
  #(parameter logic [15:0] WTIME = 16'h364,
    parameter I_BYTES = 1,
    parameter O_BYTES = 1)
  (
   input logic                  clk,
   // serial ports
   input logic                  uart_txd_in,
   output logic                 uart_rxd_out,
   ladybird_bus_interface.secondary bus,
   input logic                  nrst
   );

  localparam CNT_WIDTH = (O_BYTES == 1) ? 1 : $clog2(O_BYTES);
  logic [O_BYTES*8-1:0]         recv_data_buf;
  logic [CNT_WIDTH-1:0]         recv_cnt;
  logic                         recv_fifo_valid;
  logic                         recv_ready, recv_valid;
  logic [7:0]                   recv_data;

  logic [I_BYTES*8-1:0]         trns_data_buf;
  logic [CNT_WIDTH-1:0]         trns_cnt;
  logic                         trns_fifo_ready;
  logic                         trns_ready, trns_valid;
  logic [7:0]                   trns_data;

  generate for (genvar i = 0; i < O_BYTES; i++) begin: recieve_buffer_packing
    always_ff @(posedge clk) begin
      if (~nrst) begin
        recv_data_buf[i*8+:8] <= '0;
      end else begin
        if (i == recv_cnt) begin
          recv_data_buf[i*8+:8] <= recv_data;
        end
      end
    end
  end endgenerate

  always_ff @(posedge clk) begin
    if (~nrst) begin
      recv_fifo_valid <= '0;
    end else begin
      recv_fifo_valid <= (recv_cnt == O_BYTES-1 && recv_valid) ? 'b1 : 'b0;
    end
  end

  always_comb begin
    trns_data = trns_data_buf[trns_cnt*8+:8];
    trns_fifo_ready = (trns_cnt == I_BYTES-1 && trns_ready) ? 'b1 : 'b0;
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      recv_cnt <= 'd0;
    end else begin
      if (recv_valid && recv_cnt == O_BYTES-1) begin
        recv_cnt <= 'd0;
      end else if (recv_valid && recv_ready) begin
        recv_cnt <= recv_cnt + 'd1;
      end else begin
        recv_cnt <= recv_cnt;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      trns_cnt <= 'd0;
    end else begin
      if (trns_ready && trns_cnt == I_BYTES-1) begin
        trns_cnt <= 'd0;
      end else if (trns_ready && trns_valid) begin
        trns_cnt <= trns_cnt + 'd1;
      end
    end
  end

  // interface control
  logic read_req, read_ready;
  logic read_data_req, read_data_valid;
  logic write_req, write_ready;
  logic [O_BYTES*8-1:0] read_data;
  logic [I_BYTES*8-1:0] write_data;

  assign write_data = bus.wdata[I_BYTES*8-1:0];
  assign bus.rdata = {24'd0, read_data};
  assign read_req = bus.req & ~(|bus.wstrb);
  assign write_req = bus.req & (|bus.wstrb);
  assign bus.gnt = (read_req & read_ready) | (write_req & write_ready);
  assign bus.rdgnt = (read_data_req & read_data_valid);
  assign read_data_req = ~read_ready;

  always_ff @(posedge clk) begin
    if (~nrst) begin
      read_ready <= '1;
    end else begin
      if (read_req & read_ready) begin
        read_ready <= '0;
      end else if (bus.rdgnt) begin
        read_ready <= '1;
      end
    end
  end

  ladybird_uart_receiver #(.WTIME(WTIME))
  RECV
    (
     .clk(clk),
     .valid(recv_valid),
     .data(recv_data),
     .ready(recv_ready),
     .rx(uart_txd_in),
     .nrst(nrst)
     );

  ladybird_fifo #(.FIFO_DEPTH_W(3), .DATA_W(8*O_BYTES))
  RECV_FIFO
    (
     .clk(clk),
     .a_data(recv_data_buf),
     .a_valid(recv_fifo_valid),
     .a_ready(recv_ready),
     .b_data(read_data),
     .b_valid(read_data_valid),
     .b_ready(read_data_req),
     .nrst(nrst)
     );

  ladybird_uart_transmitter #(.WTIME(WTIME))
  TRNS
    (
     .clk(clk),
     .valid(trns_valid),
     .data(trns_data),
     .ready(trns_ready),
     .tx(uart_rxd_out),
     .nrst(nrst)
     );

  ladybird_fifo #(.FIFO_DEPTH_W(3), .DATA_W(8*I_BYTES))
  TRNS_FIFO
    (
     .clk(clk),
     .a_data(write_data),
     .a_valid(write_req),
     .a_ready(write_ready),
     .b_data(trns_data_buf),
     .b_valid(trns_valid),
     .b_ready(trns_fifo_ready),
     .nrst(nrst)
     );

endmodule
