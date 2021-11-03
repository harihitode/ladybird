`timescale 1 ns / 1 ps

module ladybird_inst_ram
  import ladybird_config::*;
  #(
    parameter DISTRIBUTED_RAM = 1
    )
  (
   input logic clk,
   ladybird_bus.secondary bus,
   input logic nrst,
   input logic anrst
   );

  logic [XLEN-1:0] data_out;
  logic [XLEN-1:0] data_in;
  assign bus.gnt = 'b1;

  generate if (DISTRIBUTED_RAM == 1) begin: DIST_IMPL
    assign bus.data = (bus.data_gnt) ? data_out : 'z;
    assign data_in = bus.data;
    localparam   ADDR_W = 4;
    logic [XLEN-1:0] ram [2**ADDR_W];
    logic            data_gnt;
    assign bus.data_gnt = data_gnt;

    automatic logic [XLEN-1:0] ram_vector [2**ADDR_W]
      = '{
          0:LUI(5'd1, 20'hfffff),
          1:SRAI(5'd1, 5'd1, 5'd12),
          2:LB(5'd2, 12'h000, 5'd1),
          3:LUI(5'd3, 20'h90000),
          4:EBREAK(),
          default:NOP()
          };
    always_ff @(posedge clk, negedge anrst) begin
      if (~anrst) begin
        data_gnt <= '0;
        data_out <= '0;
        ram <= ram_vector;
      end else begin
        if (~nrst) begin
          data_gnt <= '0;
          data_out <= '0;
          ram <= ram_vector;
        end else begin
          if (bus.req & (|bus.wstrb)) begin
            data_gnt <= '0;
            for (int i = 0; i < 4; i++) begin
              if (bus.wstrb[i]) ram[bus.addr[ADDR_W+2-1:2]][8*i+:8] <= data_in[8*i+:8];
            end
          end else if (bus.req) begin
            data_gnt <= 'b1;
            data_out <= ram[bus.addr[ADDR_W+2-1:2]];
          end else begin
            data_gnt <= 'b0;
          end
        end
      end
    end

  end else begin: BRAM_IMPL
    assign bus.data = (bus.data_gnt) ? {data_out[7:0], data_out[15:8], data_out[23:16], data_out[31:24]} : 'z;
    assign data_in = {bus.data[7:0], bus.data[15:8], bus.data[23:16], bus.data[31:24]};
    // 2 read latency
    localparam ADDR_W = 15;
    localparam READ_LATENCY = 2;
    logic [READ_LATENCY-1:0] req_l;
    logic [3:0]              wstrb;
    assign wstrb = (bus.req & bus.gnt) ? {bus.wstrb[0], bus.wstrb[1], bus.wstrb[2], bus.wstrb[3]} : '0;
    assign bus.data_gnt = req_l[0];
    always_ff @(posedge clk, negedge anrst) begin
      if (~anrst) begin
        req_l <= '0;
      end else begin
        if (~nrst) begin
          req_l <= '0;
        end else begin
          req_l <= {bus.req & ~|bus.wstrb, req_l[$high(req_l):1]};
        end
      end
    end
    // IRAM_IMPL in order to write instructions at first
    blk_mem_gen_0_iram IRAM
      (
       .addra(bus.addr[ADDR_W+2-1:2]),
       .clka(clk),
       .dina(data_in),
       .douta(data_out),
       .ena(1'b1),
       .wea(wstrb)
       );
  end endgenerate

endmodule
