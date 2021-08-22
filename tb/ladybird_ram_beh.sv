module ladybird_ram_beh
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
  logic [ADDR_W-1:0] addr;

  assign addr = bus.addr[ADDR_W+2-1:2];
  assign bus.data = rdData;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      ram <= '{default:'0};
      bus.gnt <= 'b0;
      rdData <= 'z;
    end else begin
      if (~nrst) begin
        ram <= '{default:'0};
        bus.gnt <= 'b0;
        rdData <= 'z;
      end else begin
        if (bus.req && bus.wstrb != '0) begin
          bus.gnt <= 'b1;
          rdData <= 'z;
          for (int i = 0; i < 4; i++) begin
            if (bus.wstrb[i]) ram[addr][8*i+:8] <= bus.data[8*i+:8];
          end
        end else if (bus.req) begin
          bus.gnt <= 'b1;
          rdData <= ram[addr];
        end else begin
          rdData <= 'z;
          bus.gnt <= 'b0;
        end
      end
    end
  end

endmodule
