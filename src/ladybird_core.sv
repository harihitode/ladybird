`timescale 1 ns / 1 ps

module ladybird_core
  import ladybird_config::*;
  #(parameter SIMULATION = 0)
  (
   input logic       clk,
   interface inst,
   interface data,
   input logic       anrst,
   input logic       nrst
   );

  logic [XLEN-1:0]         pc;

  assign inst.primary.data = 'z;

  always_ff @(posedge clk, negedge anrst) begin
    if (~anrst) begin
      pc <= '0;
      inst.primary.wstrb <= '0;
      inst.primary.req <= '0;
      inst.primary.addr <= '0;
    end else begin
      if (~nrst) begin
        pc <= '0;
        inst.primary.wstrb <= '0;
        inst.primary.req <= '0;
        inst.primary.addr <= '0;
      end else begin
        inst.primary.wstrb <= '0;
        inst.primary.req <= 'b1;
        inst.primary.addr <= pc;
        pc <= pc + 'd4;
        if (inst.primary.gnt) begin
          $display(">> %x", inst.primary.data);
        end
      end
    end
  end

endmodule
