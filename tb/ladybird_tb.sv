`timescale 1 ns / 1 ps

`include "../src/ladybird_riscv_helper.svh"
`include "../src/ladybird_config.svh"
`include "ladybird_elfreader.svh"

module ladybird_tb
(
  input string ELF_PATH
);
  import ladybird_riscv_helper::*;
  import ladybird_config::*;
  ladybird_elfreader elf = new(ELF_PATH);
  initial begin
    automatic logic [7:0] mem [];
    $display("LADYBIRD SIMULATION");
    $display("\t VERSION 0x%08x", VERSION);
    elf.disp_header();
    $finish;
  end
endmodule
