`ifndef LADYBIRD_CONFIG_SVH
`define LADYBIRD_CONFIG_SVH

`timescale 1 ns / 1 ps

package ladybird_config;
  // verilator lint_off UNUSED
  localparam logic [31:0] VERSION = 32'hcafe_0a0a;
  localparam              XLEN = 32;
  localparam string       VENDOR_NAME = "harihitode";
  localparam string       ARCH_NAME = "ladybird";
  localparam string       RISCV_EXTENSION_STRING = "RV32IMA";

  // core bus configurations
  localparam BUS_DATA_W = 32;
  localparam BUS_ADDR_W = 32;
  localparam BUS_ID_I = 0;
  localparam BUS_ID_D = 1;

  // core cache configurations
  localparam L1I_CACHE_INDEX_W = 5;
  localparam L1I_CACHE_LINE_W = 128;
  localparam L1D_CACHE_INDEX_W = 8;
  localparam L1D_CACHE_LINE_W = 128;

  // platform configurations
  localparam              NUM_PERIPHERAL = 6;
  typedef enum            logic [2:0] {
                                       ROM  = 3'b001,
                                       DRAM = 3'b010,
                                       UART = 3'b011,
                                       QSPI = 3'b100,
                                       GPIO = 3'b101
                                       } access_t;

  localparam MEMORY_BASEADDR_CONFROM = 32'h00001000;
  localparam MEMORY_BASEADDR_RAM = 32'h80000000;
  localparam MEMORY_BASEADDR_ACLINT = 32'h02000000;
  localparam MEMORY_SIZE_RAM = (128 * 1024 * 1024);
  localparam ACLINT_MSIP_BASE = MEMORY_BASEADDR_ACLINT;
  localparam ACLINT_MTIMECMP_BASE = MEMORY_BASEADDR_ACLINT + 32'h00004000;
  localparam ACLINT_SETSSIP_BASE = MEMORY_BASEADDR_ACLINT + 32'h00008000;
  localparam ACLINT_MTIME_BASE = MEMORY_BASEADDR_ACLINT + 32'h0000bff8;
`ifdef  LADYBIRD_SIMULATION_HTIF
  localparam MEMORY_HTIF_TOHOST = 32'h80001000;
  localparam MEMORY_HTIF_FROMHOST = 32'h80001040;
`endif

  function automatic access_t ACCESS_TYPE(input logic [XLEN-1:0] addr);
    if (addr[XLEN-1] == '1) begin
      return DRAM;
    end else if (addr[XLEN-1:12] == 20'h00001) begin
      return ROM;
    end else begin
      return GPIO;
    end
  endfunction

  function automatic logic IS_UNCACHABLE(input logic [XLEN-1:0] addr);
    case (ACCESS_TYPE(addr))
      DRAM: begin
`ifdef LADYBIRD_SIMULATION_HTIF
        if (addr[31:12] == MEMORY_HTIF_TOHOST[31:12] ||
            addr[31:12] == MEMORY_HTIF_FROMHOST[31:12]) begin
          return '1;
        end else begin
          return '0;
        end
`else
        return '0;
`endif
      end
      ROM: begin
        return '0;
      end
      default: return '1;
    endcase
  endfunction
  // verilator lint_on UNUSED
endpackage

`endif
