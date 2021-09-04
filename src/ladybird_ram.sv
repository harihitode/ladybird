module ladybird_ram
  import ladybird_config::*;
  #(
    parameter DATA_W = 32,
    parameter ADDR_W = 10
    )
  (
   input logic clk,
   ladybird_bus.secondary bus,
   input logic nrst,
   input logic anrst
   );

  logic [DATA_W-1:0] ram [2**ADDR_W];
  logic [DATA_W-1:0] rdData;
  logic [DATA_W-1:0] wrData;
  logic              rdValid;
  logic [DATA_W/8-1:0] wrStrb;
  logic [ADDR_W-1:0] addr;

  assign addr = bus.addr[ADDR_W+2-1:2];
  assign wrData = bus.data;
  assign wrStrb = bus.wstrb;
  assign bus.data = rdData;
  assign bus.gnt = 'b1;
  assign bus.data_gnt = rdValid;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      rdValid <= 'b0;
      rdData <= 'z;
      ram <= '{
               0:ADDI(5'd1, 5'd0, 12'hfff),
               1:LB(5'd2, 12'h000, 5'd1),
               2:ADDI(5'd2, 5'd2, 12'h001),
               3:SB(5'd2, 12'h000, 5'd1),
               4:JAL(5'd0, -21'd12),
               default:32'd0
               };
    end else begin
      if (~nrst) begin
        rdValid <= 'b0;
        rdData <= 'z;
        ram <= '{
                 0:ADDI(5'd1, 5'd0, 12'hfff),
                 1:LB(5'd2, 12'h000, 5'd1),
                 2:ADDI(5'd2, 5'd2, 12'h001),
                 3:SB(5'd2, 12'h000, 5'd1),
                 4:JAL(5'd0, -21'd12),
                 default:32'd0
                 };
      end else begin
        if (bus.req && (bus.wstrb != '0)) begin
          rdValid <= 'b0;
          rdData <= 'z;
          for (int i = 0; i < 4; i++) begin
            if (wrStrb[i]) ram[addr][8*i+:8] <= wrData[8*i+:8];
          end
        end else if (bus.req) begin
          rdValid <= 'b1;
          rdData <= ram[addr];
        end else begin
          rdValid <= 'b0;
          rdData <= 'z;
        end
      end
    end
  end

endmodule
