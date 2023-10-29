`ifndef LADYBIRD_ELFREADER_SVH
`define LADYBIRD_ELFREADER_SVH

`timescale 1 ns / 1 ps

typedef logic [31:0] Elf32_Word;
typedef logic [15:0] Elf32_Half;
typedef logic [31:0] Elf32_Addr;
typedef logic [31:0] Elf32_Off;

class ladybird_elfreader;
  // verilator lint_off UNUSEDSIGNAL
  localparam EI_NINDENT = 16;
  localparam PT_LOAD = 1;
  string filepath_i;

  typedef struct packed {
    Elf32_Half  e_shstrndx;
    Elf32_Half  e_shnum;
    Elf32_Half  e_shentsize;
    Elf32_Half  e_phnum;
    Elf32_Half  e_phentsize;
    Elf32_Half  e_ehsize;
    Elf32_Word  e_flags;
    Elf32_Off   e_shoff;
    Elf32_Off   e_phoff;
    Elf32_Addr  e_entry;
    Elf32_Word  e_version;
    Elf32_Half  e_machine;
    Elf32_Half  e_type;
    logic [EI_NINDENT-1:0][7:0] e_ident;
  } Elf32_Ehdr;

  // reversed
  typedef struct packed {
    Elf32_Word p_align;
    Elf32_Word p_flags;
    Elf32_Word p_memsz;
    Elf32_Word p_filesz;
    Elf32_Addr p_paddr;
    Elf32_Addr p_vaddr;
    Elf32_Off p_offset;
    Elf32_Word p_type;
  } Elf32_Phdr;

  Elf32_Ehdr eheader_i;
  Elf32_Phdr pheader_i [];

  function new(string filepath);
    automatic int fd, cd;
    automatic logic [31:0] entry, phoff;
    automatic logic [15:0] phentsize, phnum;
    automatic logic [$size(Elf32_Phdr)-1:0] pheader;
    automatic logic [$size(Elf32_Ehdr)-1:0] eheader;
    filepath_i = filepath;
    fd = $fopen(filepath_i, "rb");
    cd = $fread(eheader, fd);
    if (cd == 0) begin
      $display("Could not read ELF %s", filepath);
    end
    eheader = {<<8{eheader}};
    eheader_i = eheader;
    $fseek(fd, eheader_i.e_phoff, 0);
    pheader_i = new[int'(eheader_i.e_phnum)];
    for (int i = 0; i < eheader_i.e_phnum; i++) begin
      cd = $fread(pheader, fd);
      pheader = {<<8{pheader}};
      pheader_i[i] = pheader;
    end
    $fclose(fd);
  endfunction

  function void disp_header();
    $display("ELF path: %s", filepath_i);
    $display("ELF Header:");
    $write("\tMagic:\t");
    for (int i = 0; i < EI_NINDENT; i++) begin
      $write("%02x ", eheader_i.e_ident[i]);
    end
    $write("\n");
    $display("\tEntry:\t0x%08x", eheader_i.e_entry);
    $display("\tStart of program headers:\t%d (bytes into file)", eheader_i.e_phoff);
    $display("\tSize of program headers:\t%d (bytes)", eheader_i.e_phentsize);
    $display("\tNumber of program headers:\t%d", eheader_i.e_phnum);
    $display("Program Headers: (Loadable)");
    $display("\tOffset\tVirtAddr\tPhysAddr\tFileSiz\tMemSiz");
    for (int i = 0; i < eheader_i.e_phnum; i++) begin
      if (pheader_i[i].p_type == PT_LOAD) begin
        $display("0x%08x\t0x%08x\t0x%08x\t0x%08x\t0x%08x",
                 pheader_i[i].p_offset,
                 pheader_i[i].p_vaddr,
                 pheader_i[i].p_paddr,
                 pheader_i[i].p_filesz,
                 pheader_i[i].p_memsz
                 );
      end
    end
  endfunction

  function void read_program(int p, output logic [7:0] mem []);
    automatic int fd;
    automatic logic [7:0] c;
    mem = new[int'(pheader_i[p].p_memsz)];
    fd = $fopen(filepath_i, "rb");
    $fseek(fd, pheader_i[p].p_offset, 0);
    for (int i = 0; i < pheader_i[p].p_filesz; i++) begin
      $fread(c, fd);
      mem[i] = c;
    end
    $fclose(fd);
  endfunction

  // verilator lint_on UNUSEDSIGNAL
endclass

`endif
