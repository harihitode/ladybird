`timescale 1 ns / 1 ps

module ladybird_uart_transmitter
  #(parameter logic [15:0] WTIME = 16'h28B0)
  (
   input logic       clk,
   input logic       valid,
   input logic [7:0] data,
   output logic      ready,
   output logic      tx,
   input logic       nrst
   );

  localparam logic [3:0] SLEEP = 4'b1111;
  localparam logic [3:0] SEND_FIRSTDATA = 4'b0000;
  localparam logic [3:0] SEND_LASTDATA = 4'b1001;

  logic [9:0]            buff;
  logic [3:0]            state;
  logic [19:0]           counter;

  always_comb begin
    tx = buff[0];
    if (state == SEND_LASTDATA && counter == '0) begin
      ready = 'b1;
    end else begin
      ready = &state;
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      buff <= '1;
    end else begin
      if (valid & ready) buff <= {1'b1, data, 1'b0};
      else if (counter == 'd0) buff <= {1'b1, buff[9:1]}; // right shift
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      state <= SLEEP;
    end else begin
      // corner case
      if (valid & ready) begin
        state <= SEND_FIRSTDATA;
      end else if (state == SEND_LASTDATA && counter == '0) begin
        state <= SLEEP;
      end else if (state < SEND_LASTDATA && counter == '0) begin
        state <= state + 'd1;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      counter <= WTIME;
    end else begin
      if (valid & ready) begin
        counter <= WTIME;
      end else if (~(|counter)) begin
        counter <= WTIME;
      end else begin
        counter <= counter - 'd1;
      end
    end
  end

endmodule
