// UART transmitter

`timescale 1 ns / 1 ps

module ladybird_uart_transmitter
  #(parameter logic [15:0] WTIME = 16'h28B0)
   (
    input logic       clk,
    input logic       valid,
    input logic [7:0] data,
    output logic      ready,
    output logic      tx,
    input logic       anrst,
    input logic       nrst
    );

   localparam logic [3:0] SLEEP = 4'b1111;
   localparam logic [3:0] SEND_FIRSTDATA = 4'b0000;
   localparam logic [3:0] SEND_LASTDATA = 4'b1001;

   logic [9:0]            buff = '1;
   logic [3:0]            state = SLEEP;
   logic [19:0]           counter = WTIME;

   assign tx = buff[0];

   always_comb
     if (state == SEND_LASTDATA && counter == 0) ready = 'b1;
     else ready = &state;

   always_ff @(posedge clk) begin
      if (nrst) begin
         if (valid & ready) buff <= {1'b1, data, 1'b0};
         else if (counter == 'd0) buff <= {1'b1, buff[9:1]}; // right shift
      end else begin
         buff <= '1;
      end
   end

   always_ff @(posedge clk) begin
      if (nrst) begin
         // corner case
         if (valid & ready) state <= SEND_FIRSTDATA;
         else if (state == SEND_LASTDATA && counter == 0) state <= SLEEP;
         else if (state < SEND_LASTDATA && counter == 0) state <= state + 1;
      end else begin
         state <= SLEEP;
      end
   end

   always_ff @(posedge clk) begin
      if (nrst) begin
         if (valid & ready) counter <= WTIME;
         else if (~(|counter)) counter <= WTIME;
         else counter <= counter - 1;
      end else begin
         counter <= WTIME;
      end
   end

endmodule
