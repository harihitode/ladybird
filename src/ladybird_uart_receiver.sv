`timescale 1 ns / 1 ps

module ladybird_uart_receiver
  #(parameter logic [15:0] WTIME = 16'h28B0)
  (
   input logic        clk,
   output logic       valid,
   output logic [7:0] data,
   input logic        ready,
   input logic        rx,
   input logic        nrst
   );

  localparam logic    SLEEP = 'b0;
  localparam logic    RUNNING = 'b1;

  logic               statemachine_start, statemachine_countup, statemachine_receive;
  logic               state, next_state;
  logic [7:0]         current_buff;
  logic [19:0]        counter, next_counter;
  logic [3:0]         index, next_index;
  logic               waitsleep, next_waitsleep;
  logic               reading, next_reading;
  logic               reset;
  logic               rx_l, rx_ll;
  logic [7:0]         data_i;
  logic               valid_i;

  assign data = data_i;
  assign valid = valid_i;

  assign statemachine_start = (~state) & (~rx_ll); // read start bit
  assign reset = (waitsleep) & rx_ll;                // read stop bit
  assign statemachine_receive = (index == 8) ? reading & statemachine_countup : 0;
  assign statemachine_countup = (counter == 0) ? 1'b1 : 1'b0; // counter == 0

  assign next_counter = (reset || state == SLEEP) ? {6'd0, WTIME[15:1]} :
                        (statemachine_countup) ? {5'd0, WTIME[15:0]} :
                        counter + 20'hfffff;

  assign next_state = (statemachine_start) ? RUNNING :
                      (reset)              ? SLEEP :
                      state;

  assign next_index = (statemachine_start)   ? 3'b000 :
                      (statemachine_countup) ? index + 1 :
                      index;

  assign next_reading = (statemachine_start) ? 1'b1 :
                        (reset)              ? 1'b0 :
                        reading;

  assign next_waitsleep = (statemachine_receive) ? 'b1 :
                          (waitsleep & rx_ll)    ? 'b0 :
                          waitsleep;

  always_ff @(posedge clk) begin
    if (~nrst) begin
      rx_l <= 'b1;
      rx_ll <= 'b1;
      state <= SLEEP;
      counter <= {5'd0, WTIME[15:1]};
      index <= 1'b0;
      waitsleep <= 1'b0;
      reading <= 1'b0;
      valid_i <= 1'b0;
      data_i <= 8'hff;
      current_buff = 8'hff;
    end else begin
      rx_l <= rx;
      rx_ll <= rx_l;
      state <= next_state;
      counter <= next_counter;
      index <= next_index;
      waitsleep <= next_waitsleep;
      reading <= next_reading;
      if (ready & valid) begin
        valid_i <= 1'b0;
      end else if (reset & ~valid) begin
        valid_i <= 1'b1;
        data_i <= current_buff;
      end
      if (statemachine_countup) current_buff <= {rx, current_buff[7:1]};
    end
  end

endmodule
