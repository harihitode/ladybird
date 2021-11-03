`timescale 1 ns / 1 ps
`default_nettype none
module ladybird_qspi_interface
  (
   input wire       sck,
   output logic     cs,
   inout wire [3:0] dq,
   ladybird_bus.secondary bus,
   input wire       nrst
   );

  typedef enum      logic [1:0] {
                                 IDLE,
                                 COMMAND,
                                 READING,
                                 WAITREADY
                                 } state_t;
  state_t           state;
  // data and command limit
  logic [7:0]       counter, command_limit, data_limit;
  logic [7:0]       command_limit_i, data_limit_i;
  // command and data value
  logic [63:0]      command, command_i;
  logic [7:0]       rd_data;

  assign dq[0] = command[$high(command)];
  assign dq[1] = 1'bz;
  assign dq[2] = 1'b1;
  assign dq[3] = 1'b1;
  assign cs = ((state == COMMAND) || (state == READING)) ? 1'b0 : 1'b1;
  // bus control
  assign bus.gnt = (state == IDLE) ? 1'b1 : 1'b0;
  assign bus.data_gnt = (state == WAITREADY) ? 1'b1 : 1'b0;
  assign bus.data = (state == WAITREADY) ? {4{rd_data}} : 'z;

  // this is specific to each SPI flash
  localparam logic [7:0] CMD_REMS = 8'h90;
  localparam logic [7:0] CMD_RDCR = 8'h35;
  localparam logic [7:0] CMD_RDSR = 8'h05;
  localparam logic [7:0] CMD_WRR = 8'h01;
  localparam logic [7:0] CMD_WREN = 7'h06;
  localparam logic [7:0] CMD_FAST_READ = 8'h0c;
  localparam logic [7:0] CMD_PAGE_PRAGRAM = 8'h12;

  always_ff @(posedge sck) begin
    if (~nrst) begin
      state <= IDLE;
    end else begin
      case (state)
        COMMAND: begin
          if (counter == command_limit) begin
            if (data_limit == '0) begin
              state <= IDLE;
            end else begin
              state <= READING;
            end
          end
        end
        READING: begin
          if (counter == data_limit) begin
            state <= WAITREADY;
          end
        end
        WAITREADY: begin
          state <= IDLE;
        end
        default: begin
          if (bus.req & bus.gnt) begin
            if (command_limit_i != '0) begin
              state <= COMMAND;
            end
          end
        end
      endcase
    end
  end

  always_comb begin: address_command_converter
    if (bus.addr[16] == 'b1) begin
      if (|bus.wstrb) begin
        case (bus.addr[2:0])
          3'b100: begin
            command_i = {CMD_WRR, 8'h00, 8'h80, 40'd0};
            command_limit_i = 'd23;
          end
          3'b101: begin
            command_i = {CMD_WREN, 56'd0};
            command_limit_i = 'd7;
          end
          default: begin
            command_i = {CMD_WREN, 56'd0};
            command_limit_i = 'd0;
          end
        endcase
        data_limit_i = 'd0;
      end else begin
        case (bus.addr[2:0])
          3'b000: begin
            command_i = {CMD_REMS, 24'h000000, 32'd0};
            command_limit_i = 'd31;
          end
          3'b001: begin
            command_i = {CMD_REMS, 24'h000001, 32'd0};
            command_limit_i = 'd31;
          end
          3'b010: begin
            command_i = {CMD_RDSR, 24'd0, 32'd0};
            command_limit_i = 'd7;
          end
          3'b011: begin
            command_i = {CMD_RDCR, 24'd0, 32'd0};
            command_limit_i = 'd7;
          end
          default: begin
            command_i = {CMD_REMS, 24'h000000, 32'd0};
            command_limit_i = 'd31;
          end
        endcase
        data_limit_i = 'd8;
      end
    end else begin
      if (|bus.wstrb) begin
        // write
        command_i = {CMD_PAGE_PRAGRAM, 16'd0, bus.addr[15:0], bus.data[7:0], 16'd0};
        command_limit_i = 'd47;
        data_limit_i = 'd0;
      end else begin
        // read
        command_i = {CMD_FAST_READ, 16'd0, bus.addr[15:0], 8'd0, 16'd0};
        command_limit_i = 'd47;
        data_limit_i = 'd8;
      end
    end
  end

  always_ff @(posedge sck) begin
    if (~nrst) begin
      counter <= '0;
    end else begin
      if (bus.req & bus.gnt) begin
        counter <= '0;
      end else if ((state == COMMAND) && (counter == command_limit)) begin
        counter <= '0;
      end else if ((state == READING) && (counter == data_limit)) begin
        counter <= '0;
      end else begin
        counter <= counter + 'd1;
      end
    end
  end

  always_ff @(posedge sck) begin
    if (~nrst) begin
      command_limit <= '0;
      data_limit <= '0;
      command <= '0;
      rd_data <= '0;
    end else begin
      if (bus.req & bus.gnt) begin
        command <= command_i;
        command_limit <= command_limit_i;
        data_limit <= data_limit_i;
      end else begin
        command <= {command[$high(command)-1:0], 1'b0};
      end
      if (state == READING) begin
        rd_data <= {rd_data[$high(rd_data)-1:0], dq[1]};
      end
    end
  end

endmodule

`default_nettype wire
