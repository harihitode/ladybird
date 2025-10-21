`timescale 1 ns / 1 ps

`include "../src/ladybird_riscv_helper.svh"
`include "../src/ladybird_config.svh"
`include "ladybird_elfreader.svh"

// verilator lint_off UNUSEDSIGNAL
module ladybird_tb
(
  input string ELF_PATH,
  input logic [63:0] TIMEOUT
);
  import ladybird_riscv_helper::*;
  import ladybird_config::*;
`ifdef LADYBIRD_SIM_TARGET_ELF
  ladybird_elfreader elf = new(LADYBIRD_SIM_TARGET_ELF);
`else
  ladybird_elfreader elf = new(ELF_PATH);
`endif

  logic [63:0]       timeout;
`ifdef LADYBIRD_SIM_TIMEOUT
  assign timeout = LADYBIRD_SIM_TIMEOUT;
`else
  assign timeout = TIMEOUT;
`endif

  logic        clk = '0;
  initial forever #5 clk = ~clk;

  logic        start = '0;
  logic [XLEN-1:0] start_pc = '0;
  logic            pending;
  logic            complete;
  logic            nrst = '0;

  localparam AXI_DATA_W = 32;
  localparam AXI_ADDR_W = 32;

  ladybird_axi_interface #(.AXI_DATA_W(AXI_DATA_W), .AXI_ADDR_W(AXI_ADDR_W)) axi(.aclk(clk));

  ladybird_core
  CORE (
        .clk(clk),
        .start(start),
        .start_pc(start_pc),
        .axi(axi),
        .pending(pending),
        .complete(complete),
        .nrst(nrst)
        );

  ladybird_simulation_memory
  MEMORY (
          .clk(clk),
          .axi(axi.slave),
          .nrst(nrst)
          );

  initial begin
    $display("LADYBIRD SIMULATION");
    $display("\t VERSION 0x%08x", VERSION);
    pending = '0;
    nrst = '0;
    #100;
    nrst = '1;
    elf.disp_header();
    for (int p = 0; p < elf.eheader_i.e_phnum; p++) begin
      automatic logic [7:0] prog [];
      elf.read_program(p, prog);
      for (int i = 0; i < prog.size(); i++) begin
        MEMORY.write(elf.pheader_i[p].p_paddr + i, prog[i]);
      end
    end
    start_pc = elf.eheader_i.e_entry;
    #10;
    start = 'b1;
    while ($time < timeout) begin
      @(posedge clk);
      if (complete == 'b1) begin
        break;
      end
    end
    $display("timeout %08x", timeout);
    $finish;
  end
endmodule
// verilator lint_on UNUSEDSIGNAL
