`timescale 1 ns / 1 ps
module ladybird_crossbar
  import ladybird_config::*;
  #(
    parameter N_CORE_BUS = 2,
    parameter N_PERIPHERAL_BUS = 4
    )
  (
   input logic clk,
   interface.secondary core_ports [N_CORE_BUS],
   interface.primary peripheral_ports [N_PERIPHERAL_BUS],
   input logic nrst,
   input logic anrst
   );

  function automatic access_t ACCESS_TYPE(input logic [XLEN-1:0] addr);
    case (addr[31:28])
      4'hF:    return UART;
      4'h8:    return DISTRIBUTED_RAM;
      4'h9:    return BLOCK_RAM;
      default: return DYNAMIC_RAM;
    endcase
  endfunction

  access_t [0:N_CORE_BUS-1]   access_type;

  generate for (genvar i = 0; i < N_CORE_BUS; i++) begin
    assign access_type[i] = ACCESS_TYPE(core_ports[i].addr);
  end endgenerate

  assign peripheral_ports[UART].req = core_ports[D_BUS].req;
  assign peripheral_ports[UART].addr = core_ports[D_BUS].addr;
  assign peripheral_ports[UART].wstrb = core_ports[D_BUS].wstrb;
  assign peripheral_ports[UART].data = peripheral_ports[UART].data_gnt ? 'z : core_ports[D_BUS].data;
  assign core_ports[D_BUS].gnt = peripheral_ports[UART].gnt;
  assign core_ports[D_BUS].data = peripheral_ports[UART].data_gnt ? peripheral_ports[UART].data : 'z;
  assign core_ports[D_BUS].data_gnt = peripheral_ports[UART].data_gnt;

  assign peripheral_ports[BLOCK_RAM].req = core_ports[I_BUS].req;
  assign peripheral_ports[BLOCK_RAM].addr = core_ports[I_BUS].addr;
  assign peripheral_ports[BLOCK_RAM].wstrb = core_ports[I_BUS].wstrb;
  assign peripheral_ports[BLOCK_RAM].data = peripheral_ports[BLOCK_RAM].data_gnt ? 'z : core_ports[I_BUS].data;
  assign core_ports[I_BUS].gnt = peripheral_ports[BLOCK_RAM].gnt;
  assign core_ports[I_BUS].data = peripheral_ports[BLOCK_RAM].data_gnt ? peripheral_ports[BLOCK_RAM].data : 'z;
  assign core_ports[I_BUS].data_gnt = peripheral_ports[BLOCK_RAM].data_gnt;

endmodule
