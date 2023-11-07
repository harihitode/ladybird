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
  logic            trap;
  logic            nrst = '0;

  localparam AXI_DATA_W = 32;
  localparam AXI_ADDR_W = 32;

  task config_rom_on();
    automatic logic [XLEN-1:0] conf_str_addr = MEMORY_BASEADDR_CONFROM + 'd32;
    automatic int last = 0;
    automatic string conf_str = $sformatf("platform { vendor %s; arch %s; };\nrtc { addr %08x; };\nram { 0 { addr %08x; size %08x; }; };\ncore { 0 { 0 { isa %s; timecmp %08x; ipi %08x; }; }; };\n",
                                          VENDOR_NAME, ARCH_NAME,
                                          ACLINT_MTIME_BASE,
                                          MEMORY_BASEADDR_RAM, MEMORY_SIZE_RAM,
                                          "RV32IM",
                                          ACLINT_MTIMECMP_BASE, ACLINT_MSIP_BASE);
    MEMORY.write(MEMORY_BASEADDR_CONFROM + 'd12, conf_str_addr[7:0]);
    MEMORY.write(MEMORY_BASEADDR_CONFROM + 'd13, conf_str_addr[15:8]);
    MEMORY.write(MEMORY_BASEADDR_CONFROM + 'd14, conf_str_addr[23:16]);
    MEMORY.write(MEMORY_BASEADDR_CONFROM + 'd15, conf_str_addr[31:24]);
    $display("[core config]");
    $write("%s", conf_str);
    foreach (conf_str[i]) begin
      MEMORY.write(conf_str_addr + i, conf_str[i]);
      last++;
    end
    MEMORY.write(conf_str_addr + last, '0);
  endtask

  ladybird_axi_interface #(.AXI_DATA_W(AXI_DATA_W), .AXI_ADDR_W(AXI_ADDR_W)) axi(.aclk(clk));

  ladybird_core #(.HART_ID(0))
  CORE (
        .clk(clk),
        .start(start),
        .start_pc(start_pc),
        .axi(axi),
        .trap(trap),
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
    nrst = '0;
    #100;
    nrst = '1;
    elf.disp_header();
    config_rom_on();
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
      if (trap == 'b1) begin
        break;
      end
    end
    $finish;
  end
endmodule
// verilator lint_on UNUSEDSIGNAL
