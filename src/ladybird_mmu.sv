`timescale 1 ns / 1 ps

module ladybird_mmu
  import ladybird_config::*;
  #(parameter SIMULATIUON = 0)
  (
   input logic             clk,
   input logic             i_valid,
   output logic            i_ready,
   input logic [XLEN-1:0]  i_addr,
   input logic [XLEN-1:0]  i_data,
   input logic             i_we,
   input logic [2:0]       i_funct,
   output logic            o_valid,
   input logic             o_ready,
   output logic [XLEN-1:0] o_data,
   input logic [XLEN-1:0]  pc,
   input logic             pc_valid,
   output logic            pc_ready,
   output logic [XLEN-1:0] inst,
   output logic            inst_valid,
   //
   interface.primary i_bus,
   interface.primary d_bus,
   input logic             anrst,
   input logic             nrst
   );

  typedef struct packed {
    logic              valid;
    logic              we;
    logic [2:0]        funct;
    logic [XLEN-1:0]   addr;
    logic [XLEN-1:0]   data;
  } request_t;

  request_t            request_buffer;
  logic                request_done;
  logic [XLEN-1:0]     o_addr;
  logic [XLEN/8-1:0]   o_wstrb;


  // data bus
  assign i_ready = ~request_buffer.valid;
  assign d_bus.data = (d_bus.req & request_buffer.we) ? request_buffer.data : 'z;
  assign d_bus.req = ~request_done & request_buffer.valid;
  assign d_bus.addr = o_addr;
  assign d_bus.wstrb = o_wstrb;

  assign o_valid = d_bus.data_gnt;
  assign o_data = d_bus.data;

  always_comb begin
    o_addr = request_buffer.addr;
    if (request_buffer.we) begin
      if (request_buffer.funct == 3'b000) begin
        o_wstrb = 4'b0001;
      end else if (request_buffer.funct == 3'b001) begin
        o_wstrb = 4'b0011;
      end else begin
        o_wstrb = 4'b1111;
      end
    end else begin
      o_wstrb = 4'b0000;
    end
  end

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      request_buffer <= '0;
      request_done <= '0;
    end else begin
      if (~nrst) begin
        request_buffer <= '0;
        request_done <= '0;
      end else begin
        if (request_buffer.valid & d_bus.gnt) begin
          request_done <= '1;
        end else if (request_buffer.we | (o_ready & o_valid)) begin
          request_done <= '0;
        end
        if (i_ready & i_valid) begin
          request_buffer.valid <= i_ready & i_valid;
          request_buffer.addr <= i_addr;
          request_buffer.data <= i_data;
          request_buffer.funct <= i_funct;
          request_buffer.we <= i_we;
        end else if (request_done) begin
          if (request_buffer.we | (o_valid & o_ready)) begin
            request_buffer <= '0;
          end
        end
      end
    end
  end

  // inst bus (read only)
  assign i_bus.data = 'z;
  assign i_bus.wstrb = '0;
  assign inst_valid = i_bus.data_gnt;
  assign inst = i_bus.data;
  assign pc_ready = i_bus.gnt;
  assign i_bus.addr = pc;
  assign i_bus.req = pc_valid & pc_ready;

endmodule
