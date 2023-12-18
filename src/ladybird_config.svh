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

  // core bus type
  typedef enum            logic [0:0] {
                                       D_BUS = 1'b0,
                                       I_BUS = 1'b1
                                       } core_bus_t;
  // access type
  localparam              NUM_PERIPHERAL = 6;
  typedef enum            logic [2:0] {
                                       IRAM = 3'b000,
                                       BRAM = 3'b001,
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

  function automatic access_t ACCESS_TYPE(input logic [XLEN-1:0] addr);
    case (addr[XLEN-1-:4])
      4'hF:    return UART;
      4'hE:    return GPIO;
      4'hD:    return QSPI;
      4'h8:    return BRAM;
      4'h9:    return IRAM;
      default: return DRAM;
    endcase
  endfunction
  // verilator lint_on UNUSED
endpackage

`endif
