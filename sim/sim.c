#include "sim.h"
#include "elfloader.h"
#include "core.h"
#include "memory.h"
#include "csr.h"
#include "mmio.h"
#include "plic.h"
#include "trigger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sim_init(sim_t *sim) {
  // init memory
  sim->mem = (memory_t *)malloc(sizeof(memory_t));
  memory_init(sim->mem, MEMORY_BASE_ADDR_RAM, RAM_SIZE, RAM_PAGE_SIZE);
  // init csr
  sim->csr = (csr_t *)malloc(sizeof(csr_t));
  csr_init(sim->csr);
  // init core
  sim->core = (core_t *)malloc(sizeof(core_t));
  core_init(sim->core);
  // set weak references
  sim->core->mem = sim->mem;
  sim->core->csr = sim->csr;
  /// for riscv config string ROM
  sim->config_rom = (char *)calloc(CONFIG_ROM_SIZE, sizeof(char));
  *(unsigned *)(sim->config_rom + 0x0c) = 0x00001020;
  sprintf(&sim->config_rom[32],
          "platform { vendor %s; arch %s; };\n"
          "rtc { addr %08x; };\n"
          "ram { 0 { addr %08x; size %08x; }; };\n"
          "core { 0 { 0 { isa %s; timecmp %08x; ipi %08x; }; }; };\n",
          VENDOR_NAME, ARCH_NAME, ACLINT_MTIMER_MTIME_BASE,
          MEMORY_BASE_ADDR_RAM, RAM_SIZE,
          EXTENSION_STR,
          ACLINT_MTIMER_MTIMECMP_BASE, ACLINT_SSWI_BASE); // [TODO?] MSWI_BASE?
  memory_set_rom(sim->mem, sim->config_rom, CONFIG_ROM_ADDR, CONFIG_ROM_SIZE);
  // MMIO's
  /// uart for console/file
  sim->uart = (uart_t *)malloc(sizeof(uart_t));
  uart_init(sim->uart);
  memory_set_mmio(sim->mem, (struct mmio_t *)sim->uart, MEMORY_BASE_ADDR_UART);
  /// disk
  sim->disk = (disk_t *)malloc(sizeof(disk_t));
  disk_init(sim->disk);
  sim->disk->mem = sim->mem; // for DMA
  memory_set_mmio(sim->mem, (struct mmio_t *)sim->disk, MEMORY_BASE_ADDR_DISK);
  /// platform level interrupt controller
  sim->plic = (plic_t *)malloc(sizeof(plic_t));
  plic_init(sim->plic);
  sim->plic->uart = sim->uart;
  sim->plic->disk = sim->disk;
  memory_set_mmio(sim->mem, (struct mmio_t *)sim->plic, MEMORY_BASE_ADDR_PLIC);
  /// core local interrupt module
  sim->aclint = (aclint_t *)malloc(sizeof(aclint_t));
  aclint_init(sim->aclint);
  sim->aclint->csr = sim->csr;
  memory_set_mmio(sim->mem, (struct mmio_t *)sim->aclint, MEMORY_BASE_ADDR_ACLINT);
  /// trigger module
  sim->trigger = (trigger_t *)malloc(sizeof(trigger_t));
  trig_init(sim->trigger);
  trig_resize(sim->trigger, 16);
  // set weak reference to csr
  sim->csr->mem = sim->mem;
  sim->csr->plic = sim->plic;
  sim->csr->trig = sim->trigger;
  // init register
  sim->reginfo = (char **)calloc(NUM_REGISTERS, sizeof(char *));
  for (int i = 0; i < NUM_REGISTERS; i++) {
    char *buf = (char *)malloc(128 * sizeof(char));
    sprintf(buf, "bitsize:%d;offset:%d;format:hex;dwarf:%d;set:General Purpose Registers;",
            XLEN, i * (XLEN/8), i);
    if (i < NUM_GPR) {
      sprintf(&buf[strlen(buf)], "name:x%d;", i);
      if (i == REG_RA) {
        sprintf(&buf[strlen(buf)], "alt-name:sp;generic:sp;");
      } else if (i == REG_FP) {
        sprintf(&buf[strlen(buf)], "alt-name:fp;generic:fp;");
      }
    } else if (i == REG_PC) {
      sprintf(&buf[strlen(buf)], "name:pc;drawf:%d;generic:pc;", i);
    }
    sim->reginfo[i] = buf;
  }
  sprintf(sim->triple, "%s", TARGET_TRIPLE);
  sim->dbg_handler = NULL;
  sim->state = running;
  return;
}

int sim_load_elf(sim_t *sim, const char *elf_path) {
  int ret = 1;
  unsigned dcsr = sim_read_csr(sim, CSR_ADDR_D_CSR);
  // init elf loader
  sim->elf = (elf_t *)malloc(sizeof(elf_t));
  elf_init(sim->elf, elf_path);
  if (sim->elf->status != ELF_STATUS_LOADED) {
    goto cleanup;
  }
  // program load to memory
  for (unsigned i = 0; i < sim->elf->programs; i++) {
    for (unsigned j = 0; j < sim->elf->program_mem_size[i]; j++) {
      if (j < sim->elf->program_file_size[i]) {
        memory_store(sim->mem, sim->elf->program_base[i] + j, sim->elf->program[i][j], 1, 0);
      } else {
        // zero clear for BSS
        memory_store(sim->mem, sim->elf->program_base[i] + j, 0x00, 1, 0);
      }
    }
  }
  memory_dcache_write_back(sim->mem);
  // set entry program counter
  sim->csr->pc = sim->elf->entry_address;
  sim_write_csr(sim, CSR_ADDR_D_PC, sim->elf->entry_address);
  sim_write_csr(sim, CSR_ADDR_D_CSR, (dcsr & 0xfffffffc) | PRIVILEGE_MODE_M);
  ret = 0;
 cleanup:
  elf_fini(sim->elf);
  free(sim->elf);
  sim->elf = NULL;
  return ret;
}

void sim_fini(sim_t *sim) {
  core_fini(sim->core);
  free(sim->core);
  memory_fini(sim->mem);
  free(sim->mem);
  csr_fini(sim->csr);
  free(sim->csr);
  trig_fini(sim->trigger);
  free(sim->trigger);
  uart_fini(sim->uart);
  free(sim->uart);
  disk_fini(sim->disk);
  free(sim->disk);
  plic_fini(sim->plic);
  free(sim->plic);
  for (int i = 0; i < NUM_REGISTERS; i++) {
    free(sim->reginfo[i]);
  }
  free(sim->reginfo);
  free(sim->config_rom);
  return;
}

void sim_debug(sim_t *sim, void (*callback)(sim_t *)) {
  sim->dbg_handler = callback;
  return;
}

void sim_resume(sim_t *sim) {
  sim->csr->pc = sim_read_csr(sim, CSR_ADDR_D_PC);
  sim->csr->mode = sim_read_csr(sim, CSR_ADDR_D_CSR) & 0x3;
  struct dbg_step_result result;
  while (sim->csr->mode != PRIVILEGE_MODE_D) {
    unsigned pc = sim->csr->pc;
    core_step(sim->core, pc, &result, sim->csr->mode);
    trig_cycle(sim->trigger, &result);
    csr_cycle(sim->csr, &result);
  }
  if (sim->dbg_handler) sim->dbg_handler(sim);
  return;
}

void sim_single_step(sim_t *sim) {
  unsigned dcsr = sim_read_csr(sim, CSR_ADDR_D_CSR);
  dcsr |= 0x00000004;
  sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
  sim_resume(sim);
  dcsr &= 0xfffffffb;
  sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
  return;
}

unsigned sim_read_register(sim_t *sim, unsigned regno) {
  if (regno == 0) {
    return 0;
  } else if (regno < NUM_GPR) {
    return sim->core->gpr[regno];
  } else if (regno == REG_PC) {
    return sim->csr->pc;
  } else {
    return 0;
  }
}

void sim_write_register(sim_t *sim, unsigned regno, unsigned value) {
  if (regno < NUM_GPR) {
    sim->core->gpr[regno] = value;
  } else if (regno == REG_PC) {
    sim->csr->pc = value;
  }
  return;
}

char sim_read_memory(sim_t *sim, unsigned addr) {
  unsigned prv = sim->csr->dcsr_mprven ? sim->csr->dcsr_prv : PRIVILEGE_MODE_M;
  unsigned value;
  memory_load(sim->mem, addr, &value, 1, prv);
  return (char)value;
}

void sim_write_memory(sim_t *sim, unsigned addr, char value) {
  unsigned prv = sim->csr->dcsr_mprven ? sim->csr->dcsr_prv : PRIVILEGE_MODE_M;
  memory_store(sim->mem, addr, value, 1, prv);
  // flush
  sim_cache_flush(sim);
  return;
}

void sim_cache_flush(sim_t *sim) {
  memory_dcache_write_back(sim->mem);
  memory_icache_invalidate(sim->mem);
  return;
}

int sim_virtio_disk(sim_t *sim, const char *img_path, int mode) {
  disk_load(sim->disk, img_path, mode);
  return 0;
}

int sim_uart_io(sim_t *sim, const char *in_path, const char *out_path) {
  uart_set_io(sim->uart, in_path, out_path);
  return 0;
}

unsigned sim_read_csr(sim_t *sim, unsigned addr) {
  return csr_csrr(sim->csr, addr);
}

void sim_write_csr(sim_t *sim, unsigned addr, unsigned value) {
  csr_csrw(sim->csr, addr, value);
}

void sim_debug_dump_status(sim_t *sim) {
  fprintf(stderr, "MODE: ");
  switch (sim->csr->mode) {
  case PRIVILEGE_MODE_M:
    fprintf(stderr, "Machine\n");
    break;
  case PRIVILEGE_MODE_S:
    fprintf(stderr, "Supervisor\n");
    break;
  case PRIVILEGE_MODE_U:
    fprintf(stderr, "User\n");
    break;
  case PRIVILEGE_MODE_D:
    fprintf(stderr, "Debug\n");
    break;
  default:
    fprintf(stderr, "unknown: %d\n", sim->csr->mode);
    break;
  }
  fprintf(stderr, "STATUS: %08x\n", csr_csrr(sim->csr, CSR_ADDR_M_STATUS));
  fprintf(stderr, "MIP: %08x\n", csr_csrr(sim->csr, CSR_ADDR_M_IP));
  fprintf(stderr, "MIE: %08x\n", csr_csrr(sim->csr, CSR_ADDR_M_IE));
  fprintf(stderr, "SIP: %08x\n", csr_csrr(sim->csr, CSR_ADDR_S_IP));
  fprintf(stderr, "SIE: %08x\n", csr_csrr(sim->csr, CSR_ADDR_S_IE));
  fprintf(stderr, "Instruction Count: %llu\n", sim->csr->instret);
  fprintf(stderr, "ICACHE: hit: %f%%, [%lu/%lu]\n", (double)sim->mem->icache->hit_count / sim->mem->icache->access_count, sim->mem->icache->hit_count, sim->mem->icache->access_count);
  fprintf(stderr, "DCACHE: hit: %f%%, [%lu/%lu]\n", (double)sim->mem->dcache->hit_count / sim->mem->dcache->access_count, sim->mem->dcache->hit_count, sim->mem->dcache->access_count);
  fprintf(stderr, "TLB: hit: %f%%, [%lu/%lu]\n", (double)sim->mem->tlb->hit_count / sim->mem->tlb->access_count, sim->mem->tlb->hit_count, sim->mem->tlb->access_count);
}

unsigned sim_match6(unsigned select, unsigned timing, unsigned access) {
  return ((CSR_TDATA1_TYPE_MATCH6 << CSR_TDATA1_TYPE_FIELD) |
          (1 << CSR_TDATA1_DMODE_FIELD) |
          (1 << 24) |
          (1 << 23) |
          ((select & 0x1) << 21) |
          ((timing & 0x1) << 20) |
          (CSR_TDATA1_ACTION_ENTER_DM << 12) |
          (1 << 6) |
          (1 << 4) |
          (1 << 3) |
          (access & 0x7)
          );
}

unsigned sim_icount(unsigned count) {
  return ((CSR_TDATA1_TYPE_ICOUNT << CSR_TDATA1_TYPE_FIELD) |
          (1 << CSR_TDATA1_DMODE_FIELD) |
          (1 << 26) |
          (1 << 25) |
          ((count & 0x03fff) << 10) |
          (1 << 9) |
          (1 << 7) |
          (1 << 6) |
          (CSR_TDATA1_ACTION_ENTER_DM));
}
