#include "sim.h"
#include "elfloader.h"
#include "core.h"
#include "csr.h"
#include "lsu.h"
#include "memory.h"
#include "mmio.h"
#include "plic.h"
#include "trigger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DBG_HANDLER 10

void sim_dtb_on(sim_t *sim, const char *dtb_path) {
  memory_set_rom(sim->mem, dtb_path, DEVTREE_ROM_ADDR, DEVTREE_ROM_SIZE, MEMORY_ROM_TYPE_MMAP);
}

void sim_config_on(sim_t *sim) {
  /// for riscv config string ROM
  char *config_rom = (char *)calloc(CONFIG_ROM_SIZE, sizeof(char));
  *(unsigned *)(config_rom + 0x0c) = 0x00001020;
  sprintf(&config_rom[32],
          "platform { vendor %s; arch %s; };\n"
          "rtc { addr %08x; };\n"
          "ram { 0 { addr %08x; size %08x; }; };\n"
          "core { 0 { 0 { isa %s; timecmp %08x; ipi %08x; }; }; };\n",
          VENDOR_NAME, ARCH_NAME,
          ACLINT_MTIME_BASE,
          MEMORY_BASE_ADDR_RAM, RAM_SIZE,
          riscv_get_extension_string(), ACLINT_MTIMECMP_BASE, ACLINT_MSIP_BASE);
  memory_set_rom(sim->mem, config_rom, CONFIG_ROM_ADDR, CONFIG_ROM_SIZE, MEMORY_ROM_TYPE_DEFAULT);
  free(config_rom);
}

void sim_add_core(sim_t *sim) {
  unsigned hart_id = sim->num_core;
  if (sim->core) {
    sim->core = (core_t **)realloc(sim->core, (hart_id + 1) * sizeof(core_t *));
  } else {
    sim->core = (core_t **)malloc(1 * sizeof(core_t *));
  }
  sim->core[hart_id] = (core_t *)malloc(sizeof(core_t));
  core_init(sim->core[hart_id], hart_id, sim->mem, sim->plic, sim->aclint, sim->trigger);
  ++sim->num_core;
  plic_add_hart(sim->plic);
  aclint_add_hart(sim->aclint);
}

void sim_init(sim_t *sim) {
  // init memory
  sim->mem = (memory_t *)malloc(sizeof(memory_t));
  memory_init(sim->mem, MEMORY_BASE_ADDR_RAM, RAM_SIZE, RAM_PAGE_SIZE);
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
  plic_set_peripheral(sim->plic, (struct mmio_t *)sim->uart, PLIC_UART_IRQ_NO);
  plic_set_peripheral(sim->plic, (struct mmio_t *)sim->disk, PLIC_VIRTIO_MMIO_IRQ_NO);
  memory_set_mmio(sim->mem, (struct mmio_t *)sim->plic, MEMORY_BASE_ADDR_PLIC);
  /// core local interrupt module
  sim->aclint = (aclint_t *)malloc(sizeof(aclint_t));
  aclint_init(sim->aclint);
  memory_set_mmio(sim->mem, (struct mmio_t *)sim->aclint, MEMORY_BASE_ADDR_ACLINT);
  /// trigger module
  sim->trigger = (trigger_t *)malloc(sizeof(trigger_t));
  trig_init(sim->trigger);
  // init core
  sim->core = NULL;
  sim->num_core = 0;
  sim_add_core(sim);
  // init register info
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
  sim->dbg_handler = (void (**)(sim_t *sim, unsigned, unsigned, unsigned, unsigned, unsigned))calloc(MAX_DBG_HANDLER, sizeof(void (*)(sim_t *sim, unsigned, unsigned, unsigned, unsigned, unsigned)));
  sim->stp_handler = NULL;
  sim->stp_arg = NULL;
  sim->state = running;
  sim->htif_tohost = 0;
  sim->htif_fromhost = 0;
  sim->selected_hart = 0;
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
#if 0
    printf("ELF LOAD paddr %08x len %08x filesz %08x\n", sim->elf->program_base[i], sim->elf->program_mem_size[i], sim->elf->program_file_size[i]);
#endif
    memory_dma_send(sim->mem, sim->elf->program_base[i], sim->elf->program_file_size[i], sim->elf->program[i]);
    memory_dma_send_c(sim->mem, sim->elf->program_base[i] + sim->elf->program_file_size[i],
                      sim->elf->program_mem_size[i] - sim->elf->program_file_size[i], 0);
  }
  // set entry program counter
  for (unsigned i = 0; i < sim->num_core; i++) {
    sim->core[i]->csr->pc = sim->elf->entry_address;
  }
  sim_write_csr(sim, CSR_ADDR_D_PC, sim->elf->entry_address);
  sim_write_csr(sim, CSR_ADDR_D_CSR, (dcsr & 0xfffffffc) | PRIVILEGE_MODE_M);
  ret = 0;
 cleanup:
  elf_fini(sim->elf);
  free(sim->elf);
  sim->elf = NULL;
  return ret;
}

void sim_regstat_en(sim_t *sim) {
  for (unsigned i = 0; i < sim->num_core; i++) {
    sim->core[i]->csr->regstat_en = 1;
  }
}

void sim_fini(sim_t *sim) {
  for (unsigned i = 0; i < sim->num_core; i++) {
    core_fini(sim->core[i]);
    free(sim->core[i]);
  }
  free(sim->core);
  memory_fini(sim->mem);
  free(sim->mem);
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
  free(sim->dbg_handler);
  return;
}

void sim_set_debug_callback(sim_t *sim, void (*callback)(sim_t *, unsigned, unsigned, unsigned, unsigned, unsigned)) {
  for (unsigned i = 0; i < MAX_DBG_HANDLER; i++) {
    if (sim->dbg_handler[i] == NULL) {
      sim->dbg_handler[i] = callback;
      return;
    }
  }
  fprintf(stderr, "exceeds debug handler\n");
  return;
}

void sim_set_step_callback(sim_t *sim, void (*callback)(struct core_step_result *, void *)) {
  sim->stp_handler = callback;
  return;
}

void sim_set_step_callback_arg(sim_t *sim, void *arg) {
  sim->stp_arg = arg;
}

void sim_resume(sim_t *sim) {
  sim->core[0]->csr->pc = sim_read_csr(sim, CSR_ADDR_D_PC);
  sim->core[0]->csr->mode = sim_read_csr(sim, CSR_ADDR_D_CSR) & 0x3;
  while (sim->core[0]->csr->mode != PRIVILEGE_MODE_D) {
    for (unsigned i = 0; i < sim->num_core; i++) {
      struct core_step_result result;
      memset(&result, 0, sizeof(struct core_step_result));
      unsigned pc = sim->core[i]->csr->pc;
      core_step(sim->core[i], pc, &result, sim->core[i]->csr->mode);
      trig_cycle(sim->trigger, &result);
      csr_cycle(sim->core[i]->csr, &result);
      if ((int)i == sim->selected_hart) {
        if (sim->stp_handler) sim->stp_handler(&result, sim->stp_arg);
      }
    }
    aclint_cycle(sim->aclint);
  }

  // fire debug handlers
  unsigned dcsr = sim_read_csr(sim, CSR_ADDR_D_CSR);
  unsigned cause = (dcsr >> 6) & 0x7;
  if (cause != CSR_DCSR_CAUSE_NONE) {
    unsigned trigger_type = 0;
    unsigned tdata1 = 0;
    unsigned tdata2 = 0;
    unsigned tdata3 = 0;
    dcsr |= CSR_DCSR_MPRV_EN;
    // [M mode] access to MMU -> ON
    sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
    if (cause == CSR_DCSR_CAUSE_TRIGGER) {
      int trigger_index = sim_get_trigger_fired(sim);
      if (trigger_index >= 0) {
        sim_write_csr(sim, CSR_ADDR_T_SELECT, trigger_index);
        tdata1 = sim_read_csr(sim, CSR_ADDR_T_DATA1);
        tdata2 = sim_read_csr(sim, CSR_ADDR_T_DATA2);
        tdata3 = sim_read_csr(sim, CSR_ADDR_T_DATA3);
        trigger_type = sim_get_trigger_type(tdata1);
      }
      sim_rst_trigger_hit(sim);
    }
    for (unsigned i = 0; i < MAX_DBG_HANDLER; i++) {
      if (sim->dbg_handler[i]) sim->dbg_handler[i](sim, cause, trigger_type, tdata1, tdata2, tdata3);
    }
    // [M mode] access to MMU -> OFF
    dcsr &= ~CSR_DCSR_MPRV_EN;
    sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
  }
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
    return sim->core[0]->gpr[regno];
  } else if (regno == REG_PC) {
    return sim->core[0]->csr->pc;
  } else {
    return 0;
  }
}

void sim_write_register(sim_t *sim, unsigned regno, unsigned value) {
  if (regno < NUM_GPR) {
    sim->core[0]->gpr[regno] = value;
  } else if (regno == REG_PC) {
    sim->core[0]->csr->pc = value;
  }
  return;
}

char sim_read_memory(sim_t *sim, unsigned addr) {
  struct core_step_result r;
  r.prv = sim->core[0]->csr->dcsr_mprven ? sim->core[0]->csr->dcsr_prv : PRIVILEGE_MODE_M;
  r.m_vaddr = addr;
  lsu_load(sim->core[0]->lsu, 1, &r);
  return (char)r.rd_data;
}

void sim_write_memory(sim_t *sim, unsigned addr, char value) {
  struct core_step_result r;
  r.prv = sim->core[0]->csr->dcsr_mprven ? sim->core[0]->csr->dcsr_prv : PRIVILEGE_MODE_M;
  r.m_vaddr = addr;
  r.m_data = value;
  lsu_store(sim->core[0]->lsu, 1, &r);
  // flush
  sim_cache_flush(sim);
  return;
}

void sim_cache_flush(sim_t *sim) {
  for (unsigned i = 0; i < sim->num_core; i++) {
    lsu_dcache_write_back(sim->core[i]->lsu);
    lsu_icache_invalidate(sim->core[i]->lsu);
    core_window_flush(sim->core[i]);
  }
  return;
}

static int sim_set_address_trigger(sim_t *sim, unsigned addr, unsigned access_type) {
  unsigned size = trig_size(sim->trigger);
  trig_resize(sim->trigger, size++);
  sim_write_csr(sim, CSR_ADDR_T_SELECT, size);
  sim_write_csr(sim, CSR_ADDR_T_DATA1, sim_match6(CSR_MATCH6_SELECT_ADDRESS, CSR_MATCH6_TIMING_AFTER, access_type));
  sim_write_csr(sim, CSR_ADDR_T_DATA2, addr);
  return 0;
}

int sim_set_exec_trigger(sim_t *sim, unsigned addr) {
  return sim_set_address_trigger(sim, addr, CSR_MATCH6_EXECUTE);
}

int sim_set_write_trigger(sim_t *sim, unsigned addr) {
  return sim_set_address_trigger(sim, addr, CSR_MATCH6_STORE);
};

int sim_set_read_trigger(sim_t *sim, unsigned addr) {
  return sim_set_address_trigger(sim, addr, CSR_MATCH6_LOAD);
};

int sim_set_access_trigger(sim_t *sim, unsigned addr) {
  return sim_set_address_trigger(sim, addr, CSR_MATCH6_STORE | CSR_MATCH6_LOAD);
};

int sim_rst_exec_trigger(sim_t *sim, unsigned addr) {
  return -1;
};

int sim_rst_write_trigger(sim_t *sim, unsigned addr) {
  return -1;
};

int sim_rst_read_trigger(sim_t *sim, unsigned addr) {
  return -1;
};

int sim_rst_access_trigger(sim_t *sim, unsigned addr) {
  return -1;
};

int sim_virtio_disk(sim_t *sim, const char *img_path, int mode) {
  disk_load(sim->disk, img_path, mode);
  return 0;
}

int sim_uart_io(sim_t *sim, const char *in_path, const char *out_path) {
  if (in_path != NULL || out_path != NULL) {
    uart_set_io(sim->uart, in_path, out_path);
  }
  return 0;
}

unsigned sim_read_csr(sim_t *sim, unsigned addr) {
  return csr_csrr(sim->core[0]->csr, addr, NULL);
}

unsigned long long sim_read_csr64(sim_t *sim, unsigned addrh, unsigned addrl) {
  unsigned long long result = sim_read_csr(sim, addrh);
  result = (result << 32) | (unsigned long long)sim_read_csr(sim, addrl);
  return result;
}

void sim_write_csr(sim_t *sim, unsigned addr, unsigned value) {
  csr_csrw(sim->core[0]->csr, addr, value, NULL);
}

int sim_get_trigger_fired(const sim_t *sim) {
  int size = trig_size(sim->trigger);
  for (int i = 0; i < size; i++) {
    if (sim->trigger->elem[i]->hit) {
      return i;
    }
  }
  return -1;
}

void sim_rst_trigger_hit(sim_t *sim) {
  int size = trig_size(sim->trigger);
  for (int i = 0; i < size; i++) {
    sim->trigger->elem[i]->hit = 0;
  }
}

unsigned sim_get_trigger_type(unsigned tdata1) {
  return ((tdata1 >> 28) & 0x0f);
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
