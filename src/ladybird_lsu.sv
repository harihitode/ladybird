`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_axi.svh"
`include "../src/ladybird_riscv_helper.svh"

module ladybird_lsu
  import ladybird_config::*;
  #(parameter AXI_ID = 'd1,
    parameter AXI_DATA_W = 32)
  (
   input logic             clk,
   input logic             i_valid,
   output logic            i_ready,
   input logic [XLEN-1:0]  i_addr,
   input logic [XLEN-1:0]  i_data,
   input logic             i_we,
   input logic [2:0]       i_funct,
   input logic             i_fence,
   output logic            o_valid,
   // verilator lint_off: UNUSED
   input logic             o_ready,
   // verilator lint_on: UNUSED
   output logic [XLEN-1:0] o_data,
   ladybird_axi_interface.master d_axi,
   input logic             nrst
   );

  localparam               CACHE_LINE_W = $clog2(AXI_DATA_W);
  localparam               CACHE_INDEX_W = 9;

  logic                    cache_req, cache_ready;
  logic [XLEN-1:0]         cache_addr;
  logic                    line_valid;
  logic [XLEN/8-1:0]       cache_wen;
  logic [XLEN-1:0]         cache_wdata;
  logic                    cache_uncache;
  logic                    cache_flush;
  logic [2:0]              i_funct_q;
  // verilator lint_off: UNUSED
  logic [XLEN-1:0]         line_addr;
  logic [2**CACHE_LINE_W/XLEN-1:0][XLEN-1:0] line_data;
  // verilator lint_on: UNUSED

  always_comb begin
    cache_req = i_valid;
    cache_addr = i_addr;
    cache_uncache = IS_UNCACHABLE(i_addr);
    cache_flush = i_fence;
    i_ready = cache_ready;
    o_valid = line_valid;
    cache_wen = '0;
    if (i_we) begin
      if (i_funct == ladybird_riscv_helper::FUNCT3_SB) begin
        case (i_addr[1:0])
          2'b00: cache_wen = 4'b0001;
          2'b01: cache_wen = 4'b0010;
          2'b10: cache_wen = 4'b0100;
          default: cache_wen = 4'b1000;
        endcase
      end else if (i_funct == ladybird_riscv_helper::FUNCT3_SH) begin
        if (i_addr[1] == 1'b0) begin
          cache_wen = 4'b0011;
        end else begin
          cache_wen = 4'b1100;
        end
      end else if (i_funct == ladybird_riscv_helper::FUNCT3_SW) begin
        cache_wen = 4'b1111;
      end
    end
    if (i_funct == ladybird_riscv_helper::FUNCT3_SB) begin
      cache_wdata = {4{i_data[7:0]}};
    end else if (i_funct == ladybird_riscv_helper::FUNCT3_SH) begin
      cache_wdata = {2{i_data[15:0]}};
    end else begin
      cache_wdata = i_data;
    end
  end

  always_ff @(posedge clk) begin
    if (~nrst) begin
      i_funct_q <= '0;
    end else begin
      if (cache_req & cache_ready) begin
        i_funct_q <= i_funct;
      end
    end
  end

  ladybird_cache #(.LINE_W(CACHE_LINE_W), .INDEX_W(CACHE_INDEX_W), .AXI_ID(AXI_ID), .AXI_DATA_W(AXI_DATA_W))
  DCACHE
    (
     .clk(clk),
     .i_valid(cache_req),
     .i_addr(cache_addr),
     .i_data(cache_wdata),
     .i_wen(cache_wen),
     .i_ready(cache_ready),
     .i_uncache(cache_uncache),
     .i_flush(cache_flush),
     .i_invalidate('0),
     .o_valid(line_valid),
     .o_addr(line_addr),
     .o_data(line_data),
     .axi(d_axi),
     .nrst(nrst)
     );

  always_comb begin
    automatic logic [XLEN-1:0] o_data_i = line_data[line_addr[CACHE_LINE_W-1:$clog2(XLEN/8)]];
    o_data = '0;
    if (i_funct_q == ladybird_riscv_helper::FUNCT3_LB) begin
      case (line_addr[1:0])
        2'b00: o_data = {{24{o_data_i[7]}}, o_data_i[7:0]};
        2'b01: o_data = {{24{o_data_i[15]}}, o_data_i[15:8]};
        2'b10: o_data = {{24{o_data_i[23]}}, o_data_i[23:16]};
        default: o_data = {{24{o_data_i[31]}}, o_data_i[31:24]};
      endcase
    end else if (i_funct_q == ladybird_riscv_helper::FUNCT3_LBU) begin
      case (line_addr[1:0])
        2'b00: o_data = {{24{1'b0}}, o_data_i[7:0]};
        2'b01: o_data = {{24{1'b0}}, o_data_i[15:8]};
        2'b10: o_data = {{24{1'b0}}, o_data_i[23:16]};
        default: o_data = {{24{1'b0}}, o_data_i[31:24]};
      endcase
    end else if (i_funct_q == ladybird_riscv_helper::FUNCT3_LH) begin
      if (line_addr[1] == 1'b0) begin
        o_data = {{16{o_data_i[15]}}, o_data_i[15:0]};
      end else begin
        o_data = {{16{o_data_i[31]}}, o_data_i[31:16]};
      end
    end else if (i_funct_q == ladybird_riscv_helper::FUNCT3_LHU) begin
      if (line_addr[1] == 1'b0) begin
        o_data = {{16{1'b0}}, o_data_i[15:0]};
      end else begin
        o_data = {{16{1'b0}}, o_data_i[31:16]};
      end
    end else begin: RD_LOAD_WORD
      o_data = line_data;
    end
  end

endmodule
