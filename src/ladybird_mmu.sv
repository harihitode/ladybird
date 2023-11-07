`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_axi.svh"
`include "../src/ladybird_riscv_helper.svh"

module ladybird_mmu
  import ladybird_config::*;
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
   ladybird_axi_interface.master axi,
   input logic             nrst
   );

  localparam [axi.AXI_ID_W-1:0] AXI_ID_I = 'd0;
  localparam [axi.AXI_ID_W-1:0] AXI_ID_D = 'd1;
  localparam [XLEN-1:0] address_mask = ~((axi.AXI_DATA_W / 8) - 1);

  typedef enum logic [2:0] {
                            IDLE,
                            AR_CHANNEL,
                            R_CHANNEL,
                            AW_CHANNEL,
                            W_CHANNEL,
                            B_CHANNEL
                            } state_t;
  state_t state_q, state_d;

  typedef struct packed {
    logic              valid;
    logic              we;
    logic [2:0]        funct;
    logic [XLEN-1:0]   addr;
    logic [XLEN-1:0]   data;
    logic [axi.AXI_ID_W-1:0] id;
  } request_t;

  request_t            request_q, request_d;
  logic                read_start, write_start;
  logic [XLEN-1:0]     rdata;
  logic                inst_ready = 1'b1;

  always_comb begin
    read_start = '0;
    write_start = '0;
    request_d = request_q;
    state_d = state_q;
    if (state_q == IDLE) begin
      request_d = '0;
      if (pc_valid & pc_ready) begin
        read_start = '1;
        request_d.valid = '1;
        request_d.we = '0;
        request_d.funct = ladybird_riscv_helper::FUNCT3_LW;
        request_d.addr = pc;
        request_d.data = '0;
        request_d.id = AXI_ID_I;
      end else if (i_valid & i_ready) begin
        if (i_we) begin
          write_start = '1;
        end else begin
          read_start = '1;
        end
        request_d.valid = '1;
        request_d.we = i_we;
        request_d.funct = i_funct;
        request_d.addr = i_addr;
        request_d.data = i_data;
        request_d.id = AXI_ID_D;
      end
    end
    if (state_q == IDLE) begin
      if (read_start) begin
        state_d = AR_CHANNEL;
      end else if (write_start) begin
        state_d = AW_CHANNEL;
      end
    end else if (state_q == AR_CHANNEL) begin
      if (axi.arready & axi.arvalid) begin
        state_d = R_CHANNEL;
      end
    end else if (state_q == R_CHANNEL) begin
      if (axi.rvalid & axi.rlast & axi.rready) begin
        state_d = IDLE;
      end
    end else if (state_q == AW_CHANNEL) begin
      if (axi.awready & axi.awvalid) begin
        state_d = W_CHANNEL;
      end
    end else if (state_q == W_CHANNEL) begin
      if (axi.wready & axi.wlast & axi.wready) begin
        state_d = B_CHANNEL;
      end
    end else if (state_q == B_CHANNEL) begin
      if (axi.bready & axi.bvalid) begin
        state_d = IDLE;
      end
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      request_q <= '0;
      state_q <= IDLE;
    end else begin
      request_q <= request_d;
      state_q <= state_d;
    end
  end

  // data bus
  assign pc_ready = (state_q == IDLE) ? '1 : '0;
  assign i_ready = (state_q == IDLE) ? ~pc_valid : '0;
  // AW channel
  assign axi.awid = request_q.id;
  assign axi.awaddr = request_q.addr & address_mask;
  assign axi.awlen = '0;
  assign axi.awsize = ladybird_axi::axi_burst_size_32;
  assign axi.awburst = ladybird_axi::axi_incrementing_burst;
  assign axi.awlock = '0;
  assign axi.awcache = '0;
  assign axi.awprot = '0;
  assign axi.awvalid = (state_q == AW_CHANNEL) ? '1 : '0;
  // W channel
  assign axi.wdata = request_q.data;
  always_comb begin
    axi.wstrb = '0;
    if (request_q.we) begin
      if (request_q.funct == ladybird_riscv_helper::FUNCT3_SB) begin
        case (request_q.addr[1:0])
          2'b00: axi.wstrb = 4'b0001;
          2'b01: axi.wstrb = 4'b0010;
          2'b10: axi.wstrb = 4'b0100;
          default: axi.wstrb = 4'b1000;
        endcase
      end else if (request_q.funct == ladybird_riscv_helper::FUNCT3_SH) begin
        if (request_q.addr[1] == 1'b0) begin
          axi.wstrb = 4'b0011;
        end else begin
          axi.wstrb = 4'b1100;
        end
      end else if (request_q.funct == ladybird_riscv_helper::FUNCT3_SW) begin
        axi.wstrb = 4'b1111;
      end
    end
  end
  assign axi.wlast = '1;
  assign axi.wvalid = (state_q == W_CHANNEL) ? '1 : '0;
  // B channel
  assign axi.bready = (state_q == B_CHANNEL) ? '1 : '0;
  // AR channel
  assign axi.arid = request_q.id;
  assign axi.araddr = request_q.addr & address_mask;
  assign axi.arlen = '0;
  assign axi.arsize = ladybird_axi::axi_burst_size_32;
  assign axi.arburst = ladybird_axi::axi_incrementing_burst;
  assign axi.arlock = '0;
  assign axi.arcache = '0;
  assign axi.arprot = '0;
  assign axi.arvalid = (state_q == AR_CHANNEL) ? '1 : '0;
  // R channel;
  assign o_valid = (state_q == R_CHANNEL && axi.rid == AXI_ID_D) ? axi.rvalid : '0;
  assign o_data = rdata;
  assign inst_valid = (state_q == R_CHANNEL && axi.rid == AXI_ID_I) ? axi.rvalid : '0;
  assign inst = rdata;
  assign axi.rready = (axi.rid == AXI_ID_I) ? inst_ready : o_ready;
  always_comb begin
    rdata = '0;
    if (request_q.funct == ladybird_riscv_helper::FUNCT3_LB) begin
      case (request_q.addr[1:0])
        2'b00: rdata = {{24{axi.rdata[7]}}, axi.rdata[7:0]};
        2'b01: rdata = {{24{axi.rdata[15]}}, axi.rdata[15:8]};
        2'b10: rdata = {{24{axi.rdata[23]}}, axi.rdata[23:16]};
        default: rdata = {{24{axi.rdata[31]}}, axi.rdata[31:24]};
      endcase
    end else if (request_q.funct == ladybird_riscv_helper::FUNCT3_LBU) begin
      case (request_q.addr[1:0])
        2'b00: rdata = {{24{1'b0}}, axi.rdata[7:0]};
        2'b01: rdata = {{24{1'b0}}, axi.rdata[15:8]};
        2'b10: rdata = {{24{1'b0}}, axi.rdata[23:16]};
        default: rdata = {{24{1'b0}}, axi.rdata[31:24]};
      endcase
    end else if (request_q.funct == ladybird_riscv_helper::FUNCT3_LB) begin
      if (request_q.addr[1] == 1'b0) begin
        rdata = {{16{axi.rdata[15]}}, axi.rdata[15:0]};
      end else begin
        rdata = {{16{axi.rdata[31]}}, axi.rdata[31:16]};
      end
    end else if (request_q.funct == ladybird_riscv_helper::FUNCT3_LBU) begin
      if (request_q.addr[1] == 1'b0) begin
        rdata = {{16{1'b0}}, axi.rdata[15:0]};
      end else begin
        rdata = {{16{1'b0}}, axi.rdata[31:16]};
      end
    end else begin: RD_LOAD_WORD
      rdata = axi.rdata;
    end
  end

endmodule
