`timescale 1 ns / 1 ps

`include "../src/ladybird_axi.svh"

// verilator lint_off UNUSED
module ladybird_simulation_memory
(
  input logic                  clk,
  ladybird_axi_interface.slave axi,
  input logic                  nrst
);

  import ladybird_axi::*;
  localparam AXI_ADDR_W = axi.AXI_ADDR_W;
  localparam AXI_DATA_W = axi.AXI_DATA_W;
  localparam AXI_ID_W = axi.AXI_ID_W;
  logic [7:0] memory_model[int unsigned];
  logic [LEN_W-1:0] current_beat;
  logic [31:0]      access_latency, preparing;

  typedef struct packed {
    logic [AXI_ID_W-1:0]   id;
    logic [AXI_ADDR_W-1:0] addr;
    logic [LEN_W-1:0]      blen;
    logic [SIZE_W-1:0]     bsize;
  } request_t;

  request_t request_d, request_q;

  typedef enum logic [2:0] {
                            IDLE,
                            PREPARE_READING,
                            READING,
                            PREPARE_WRITING,
                            WRITING,
                            RESPONSING
                            } state_t;
  state_t state_q, state_d;

  assign axi.awready = (state_q == IDLE) ? '1 : '0;
  assign axi.arready = (state_q == IDLE) ? '1 : '0;
  assign axi.bid = request_q.id;
  assign axi.rid = request_q.id;
  assign axi.rvalid = (state_q == READING) ? '1 : '0;
  assign axi.rlast = (state_q == READING && current_beat == request_q.blen) ? '1 : '0;
  assign axi.wready = (state_q == WRITING) ? '1 : '0;
  assign axi.bvalid = (state_q == RESPONSING) ? '1 : '0;

  always_comb begin
    for (int i = 0; i < AXI_DATA_W / 8; i++) begin
      axi.rdata[i*8+:8] = memory_model[request_q.addr + (AXI_DATA_W / 8 * current_beat) + i];
    end
  end

  always_comb begin
    state_d = state_q;
    request_d = request_q;
    case (state_q)
      default: begin
        if (axi.awvalid & axi.awready) begin
          state_d = PREPARE_WRITING;
          request_d.id = axi.awid;
          request_d.bsize = axi.awsize;
          request_d.blen = axi.awlen;
          request_d.addr = axi.awaddr;
        end else if (axi.arvalid & axi.arready) begin
          state_d = PREPARE_READING;
          request_d.id = axi.arid;
          request_d.bsize = axi.arsize;
          request_d.blen = axi.arlen;
          request_d.addr = axi.araddr;
        end
      end
      PREPARE_READING: begin
        if (preparing > access_latency) begin
          state_d = READING;
        end
      end
      READING: begin
        if (axi.rlast & axi.rready) begin
          state_d = IDLE;
        end
      end
      PREPARE_WRITING: begin
        if (preparing > access_latency) begin
          state_d = WRITING;
        end
      end
      WRITING: begin
        if ((axi.wlast & axi.wvalid & axi.wready) && current_beat == request_q.blen) begin
          state_d = RESPONSING;
        end
      end
      RESPONSING: begin
        if (axi.bvalid & axi.bready) begin
          state_d = IDLE;
        end
      end
    endcase
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      state_q <= IDLE;
      request_q <= '0;
      current_beat <= '0;
      access_latency <= '0;
      preparing <= '0;
    end else begin
      request_q <= request_d;
      state_q <= state_d;
      if (state_q == IDLE && ((axi.arready & axi.arvalid) | axi.awready & axi.awvalid)) begin
        access_latency <= 'd2;
      end
      if (state_q == PREPARE_READING || state_q == PREPARE_WRITING) begin
        preparing <= preparing + 'd1;
      end else begin
        preparing <= '0;
      end
      if (state_q == READING || state_q == WRITING) begin
        current_beat <= current_beat + 'd1;
      end else begin
        current_beat <= '0;
      end
      // if (state_q == READING) begin
      //   $display("LOAD: %08x %08x\n", request_q.addr, axi.rdata);
      // end
    end
  end

  // R/W helper functions
  function void write(int addr, logic [7:0] data);
    memory_model[addr] = data;
    return;
  endfunction

  function logic [7:0] read(int addr);
    if (memory_model.exists(addr) == '0) begin
      return byte'($random);
    end else begin
      return memory_model[addr];
    end
  endfunction
endmodule
//verilator lint_on UNUSED
