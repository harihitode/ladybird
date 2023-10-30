`timescale 1 ns / 1 ps

`define LADYBIRD_SIMULATION
`include "../src/ladybird_riscv_helper.svh"
`include "../src/ladybird_config.svh"
`include "ladybird_elfreader.svh"

module ladybird_tb
(
  input string ELF_PATH
);
  import ladybird_riscv_helper::*;
  import ladybird_config::*;
`ifdef LADYBIRD_SIM_TARGET_ELF
  ladybird_elfreader elf = new(LADYBIRD_SIM_TARGET_ELF);
`else
  ladybird_elfreader elf = new(ELF_PATH);
`endif
  initial begin
    automatic logic [7:0] mem [];
    $display("LADYBIRD SIMULATION");
    $display("\t VERSION 0x%08x", VERSION);
    elf.disp_header();
    for (int p = 0; p < elf.eheader_i.e_phnum; p++) begin
      elf.read_program(p, mem);
      for (int i = 0; i < mem.size() / 4; i++) begin
        automatic logic [31:0] inst;
        automatic string asmstr;
        inst = {mem[i * 4 + 3], mem[i * 4 + 2], mem[i * 4 + 1], mem[i * 4 + 0]};
        // asmstr = riscv_disas(inst);
        // $display("0x%08x %s (0x%08x)", elf.pheader_i[p].p_paddr + (i * 4), asmstr, inst);
      end
    end
    $finish;
  end
endmodule

`undef LADYBIRD_SIMULATION
