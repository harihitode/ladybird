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
   input logic             inst_ready,
   output logic [XLEN-1:0] inst_pc,
   ladybird_axi_interface.master i_axi,
   ladybird_axi_interface.master d_axi,
   input logic             nrst
   );

  localparam [i_axi.AXI_ID_W-1:0] AXI_ID_I = 'd0;
  localparam [d_axi.AXI_ID_W-1:0] AXI_ID_D = 'd1;
  localparam [XLEN-1:0] i_address_mask = ~((i_axi.AXI_DATA_W / 8) - 1);
  localparam [XLEN-1:0] d_address_mask = ~((d_axi.AXI_DATA_W / 8) - 1);

  typedef enum logic [2:0] {
                            IDLE,
                            AR_CHANNEL,
                            R_CHANNEL,
                            AW_CHANNEL,
                            W_CHANNEL,
                            B_CHANNEL
                            } state_t;
  state_t i_state_q, i_state_d;
  state_t d_state_q, d_state_d;

  typedef struct packed {
    logic              valid;
    logic              we;
    logic [2:0]        funct;
    logic [XLEN-1:0]   addr;
    logic [XLEN-1:0]   data;
  } d_request_t;

  typedef struct packed {
    logic        valid;
    logic [2:0]  funct;
    logic [XLEN-1:0] addr;
  } i_request_t;

  typedef struct             packed {
    logic                    valid;
    logic [XLEN-1:0]         addr;
    logic [XLEN-1:0]         data;
  } rddata_buf_t;

  d_request_t          d_request_q, d_request_d;
  i_request_t          i_request_q, i_request_d;
  logic [XLEN-1:0]     i_rdata, d_rdata;
  rddata_buf_t         i_buf_d, i_buf_q, d_buf_d, d_buf_q;

  always_comb begin
    automatic logic read_start;
    read_start = '0;
    i_request_d = i_request_q;
    i_state_d = i_state_q;
    if (pc_valid & pc_ready) begin
      read_start = '1;
      i_request_d.valid = '1;
      i_request_d.funct = ladybird_riscv_helper::FUNCT3_LW;
      i_request_d.addr = pc;
    end
    if (i_state_q == IDLE) begin
      if (read_start) begin
        i_state_d = AR_CHANNEL;
      end
    end else if (i_state_q == AR_CHANNEL) begin
      if (i_axi.arready & i_axi.arvalid) begin
        i_state_d = R_CHANNEL;
      end
    end else if (i_state_q == R_CHANNEL) begin
      if (i_axi.rvalid & i_axi.rlast & i_axi.rready) begin
        i_state_d = IDLE;
      end
    end
  end

  always_comb begin
    automatic logic read_start;
    automatic logic write_start;
    read_start = '0;
    write_start = '0;
    d_request_d = d_request_q;
    d_state_d = d_state_q;
    if (d_state_q == IDLE) begin
      d_request_d = '0;
      if (i_valid & i_ready) begin
        if (i_we) begin
          write_start = '1;
        end else begin
          read_start = '1;
        end
        d_request_d.valid = '1;
        d_request_d.we = i_we;
        d_request_d.funct = i_funct;
        d_request_d.addr = i_addr;
        d_request_d.data = i_data;
      end
    end
    if (d_state_q == IDLE) begin
      if (read_start) begin
        d_state_d = AR_CHANNEL;
      end else if (write_start) begin
        d_state_d = AW_CHANNEL;
      end
    end else if (d_state_q == AR_CHANNEL) begin
      if (d_axi.arready & d_axi.arvalid) begin
        d_state_d = R_CHANNEL;
      end
    end else if (d_state_q == R_CHANNEL) begin
      if (d_axi.rvalid & d_axi.rlast & d_axi.rready) begin
        d_state_d = IDLE;
      end
    end else if (d_state_q == AW_CHANNEL) begin
      if (d_axi.awready & d_axi.awvalid) begin
        d_state_d = W_CHANNEL;
      end
    end else if (d_state_q == W_CHANNEL) begin
      if (d_axi.wready & d_axi.wlast & d_axi.wready) begin
        d_state_d = B_CHANNEL;
      end
    end else if (d_state_q == B_CHANNEL) begin
      if (d_axi.bready & d_axi.bvalid) begin
        d_state_d = IDLE;
      end
    end
  end

  // data bus
  assign pc_ready = (i_state_q == IDLE) ? '1 : '0;
  assign i_ready = (d_state_q == IDLE) ? '1 : '0;

  always_comb begin
    i_buf_d = i_buf_q;
    if (i_buf_q.valid && inst_ready) begin
      i_buf_d.valid = '0;
    end else if (i_axi.rvalid && i_axi.rready) begin
      i_buf_d.valid = '1;
      i_buf_d.data = i_rdata;
      i_buf_d.addr = i_request_q.addr;
    end
  end

  always_comb begin
    d_buf_d = d_buf_q;
    if (d_buf_q.valid && o_ready) begin
      d_buf_d.valid = '0;
    end else if (d_axi.rvalid && d_axi.rready) begin
      d_buf_d.valid = '1;
      d_buf_d.data = d_rdata;
      d_buf_d.addr = d_request_q.addr;
    end
  end

  assign o_valid = d_buf_q.valid;
  assign o_data = d_buf_q.data;
  assign inst_valid = i_buf_q.valid;
  assign inst = i_buf_q.data;
  assign inst_pc = i_buf_q.addr;

  always_ff @(posedge clk) begin
    if (~nrst) begin
      i_request_q <= '0;
      i_state_q <= IDLE;
      i_buf_q <= '0;
      d_request_q <= '0;
      d_state_q <= IDLE;
      d_buf_q <= '0;
    end else begin
      i_request_q <= i_request_d;
      i_state_q <= i_state_d;
      i_buf_q <= i_buf_d;
      d_request_q <= d_request_d;
      d_state_q <= d_state_d;
      d_buf_q <= d_buf_d;
    end
  end

  // Instruction AW channel
  assign i_axi.awid = AXI_ID_I;
  assign i_axi.awaddr = i_request_q.addr & i_address_mask;
  assign i_axi.awlen = '0;
  assign i_axi.awsize = ladybird_axi::axi_burst_size_32;
  assign i_axi.awburst = ladybird_axi::axi_incrementing_burst;
  assign i_axi.awlock = '0;
  assign i_axi.awcache = '0;
  assign i_axi.awprot = '0;
  assign i_axi.awvalid = (i_state_q == AW_CHANNEL) ? '1 : '0;
  // Data AW channel
  assign d_axi.awid = AXI_ID_D;
  assign d_axi.awaddr = d_request_q.addr & d_address_mask;
  assign d_axi.awlen = '0;
  assign d_axi.awsize = ladybird_axi::axi_burst_size_32;
  assign d_axi.awburst = ladybird_axi::axi_incrementing_burst;
  assign d_axi.awlock = '0;
  assign d_axi.awcache = '0;
  assign d_axi.awprot = '0;
  assign d_axi.awvalid = (d_state_q == AW_CHANNEL) ? '1 : '0;
  // Instruction W channel
  assign i_axi.wid = AXI_ID_I;
  assign i_axi.wstrb = '0;
  assign i_axi.wdata = '0;
  assign i_axi.wlast = '1;
  assign i_axi.wvalid = '0;
  // Data W channel
  assign d_axi.wid = AXI_ID_D;
  always_comb begin
    d_axi.wstrb = '0;
    if (d_request_q.we) begin
      if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_SB) begin
        case (d_request_q.addr[1:0])
          2'b00: d_axi.wstrb = 4'b0001;
          2'b01: d_axi.wstrb = 4'b0010;
          2'b10: d_axi.wstrb = 4'b0100;
          default: d_axi.wstrb = 4'b1000;
        endcase
      end else if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_SH) begin
        if (d_request_q.addr[1] == 1'b0) begin
          d_axi.wstrb = 4'b0011;
        end else begin
          d_axi.wstrb = 4'b1100;
        end
      end else if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_SW) begin
        d_axi.wstrb = 4'b1111;
      end
    end
  end
  always_comb begin
    if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_SB) begin
      d_axi.wdata = {4{d_request_q.data[7:0]}};
    end else if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_SH) begin
      d_axi.wdata = {2{d_request_q.data[15:0]}};
    end else begin
      d_axi.wdata = d_request_q.data;
    end
  end
  assign d_axi.wlast = '1;
  assign d_axi.wvalid = (d_state_q == W_CHANNEL) ? '1 : '0;
  // Instruction B channel
  assign i_axi.bready = '0;
  // Data B channel
  assign d_axi.bready = (d_state_q == B_CHANNEL) ? '1 : '0;
  // Instruction AR channel
  assign i_axi.arid = AXI_ID_I;
  assign i_axi.araddr = i_request_q.addr & i_address_mask;
  assign i_axi.arlen = '0;
  assign i_axi.arsize = ladybird_axi::axi_burst_size_32;
  assign i_axi.arburst = ladybird_axi::axi_incrementing_burst;
  assign i_axi.arlock = '0;
  assign i_axi.arcache = '0;
  assign i_axi.arprot = '0;
  assign i_axi.arvalid = (i_state_q == AR_CHANNEL) ? '1 : '0;
  // Data AR channel
  assign d_axi.arid = AXI_ID_D;
  assign d_axi.araddr = d_request_q.addr & d_address_mask;
  assign d_axi.arlen = '0;
  assign d_axi.arsize = ladybird_axi::axi_burst_size_32;
  assign d_axi.arburst = ladybird_axi::axi_incrementing_burst;
  assign d_axi.arlock = '0;
  assign d_axi.arcache = '0;
  assign d_axi.arprot = '0;
  assign d_axi.arvalid = (d_state_q == AR_CHANNEL) ? '1 : '0;
  // Instruction R channel
  assign i_axi.rready = ~i_buf_q.valid;
  always_comb begin
    i_rdata = '0;
    if (i_request_q.funct == ladybird_riscv_helper::FUNCT3_LHU) begin
      if (i_request_q.addr[1] == 1'b0) begin
        i_rdata = {{16{1'b0}}, i_axi.rdata[15:0]};
      end else begin
        i_rdata = {{16{1'b0}}, i_axi.rdata[31:16]};
      end
    end else begin
      i_rdata = i_axi.rdata;
    end
  end
  // Data R channel
  assign d_axi.rready = ~d_buf_q.valid;
  always_comb begin
    d_rdata = '0;
    if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_LB) begin
      case (d_request_q.addr[1:0])
        2'b00: d_rdata = {{24{d_axi.rdata[7]}}, d_axi.rdata[7:0]};
        2'b01: d_rdata = {{24{d_axi.rdata[15]}}, d_axi.rdata[15:8]};
        2'b10: d_rdata = {{24{d_axi.rdata[23]}}, d_axi.rdata[23:16]};
        default: d_rdata = {{24{d_axi.rdata[31]}}, d_axi.rdata[31:24]};
      endcase
    end else if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_LBU) begin
      case (d_request_q.addr[1:0])
        2'b00: d_rdata = {{24{1'b0}}, d_axi.rdata[7:0]};
        2'b01: d_rdata = {{24{1'b0}}, d_axi.rdata[15:8]};
        2'b10: d_rdata = {{24{1'b0}}, d_axi.rdata[23:16]};
        default: d_rdata = {{24{1'b0}}, d_axi.rdata[31:24]};
      endcase
    end else if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_LH) begin
      if (d_request_q.addr[1] == 1'b0) begin
        d_rdata = {{16{d_axi.rdata[15]}}, d_axi.rdata[15:0]};
      end else begin
        d_rdata = {{16{d_axi.rdata[31]}}, d_axi.rdata[31:16]};
      end
    end else if (d_request_q.funct == ladybird_riscv_helper::FUNCT3_LHU) begin
      if (d_request_q.addr[1] == 1'b0) begin
        d_rdata = {{16{1'b0}}, d_axi.rdata[15:0]};
      end else begin
        d_rdata = {{16{1'b0}}, d_axi.rdata[31:16]};
      end
    end else begin: RD_LOAD_WORD
      d_rdata = d_axi.rdata;
    end
  end

endmodule
