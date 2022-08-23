#include "memory.h"
#include "mmio.h"
#include "plic.h"
#include "sim.h"
#include "csr.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MEMORY_NONE 0
#define MEMORY_RAM 1
#define MEMORY_UART 2
#define MEMORY_DISK 3
#define MEMORY_ACLINT 4
#define MEMORY_PLIC 5

void memory_init(memory_t *mem) {
  mem->blocks = 0;
  mem->base = NULL;
  mem->block = NULL;
  mem->reserve = NULL;
  mem->csr = NULL;
  mem->uart = (uart_t *)malloc(sizeof(uart_t));
  uart_init(mem->uart);
  mem->disk = (disk_t *)malloc(sizeof(disk_t));
  disk_init(mem->disk);
  mem->disk->mem = mem; // for DMA
  mem->plic = (plic_t *)malloc(sizeof(plic_t));
  plic_init(mem->plic);
  mem->plic->uart = mem->uart;
  mem->plic->disk = mem->disk;
  mem->vmflag = 0;
  mem->vmrppn = 0;
  mem->tlbs = 0;
  mem->tlb_key = NULL;
  mem->tlb_val = NULL;
}

void memory_set_sim(memory_t *mem, struct sim_t *sim) {
  mem->csr = sim->csr;
  return;
}

static unsigned memory_get_block_id(memory_t *mem, unsigned addr) {
  for (unsigned i = 0; i < mem->blocks; i++) {
    // search blocks allocated for the base addr
    // block size is 4KB
    if (mem->base[i] == (addr & 0xfffff000)) {
      return i;
    }
  }
  unsigned last_block = mem->blocks;
  mem->blocks++; // increment
  mem->base = (unsigned *)realloc(mem->base, mem->blocks * sizeof(unsigned));
  mem->block = (char **)realloc(mem->block, mem->blocks * sizeof(char *));
  mem->reserve = (char *)realloc(mem->reserve, mem->blocks * sizeof(char));
  mem->base[last_block] = (addr & 0xfffff000);
  mem->block[last_block] = (char *)malloc(0x00001000 * sizeof(char));
  mem->reserve[last_block] = 0;
  return last_block;
}

char *memory_get_page(memory_t *mem, unsigned addr) {
  unsigned bid = memory_get_block_id(mem, addr);
  char *page = mem->block[bid];
  mem->reserve[bid] = 0; // expire
  return page;
}

static unsigned memory_address_translation(memory_t *mem, unsigned addr) {
  if (mem->vmflag == 0 || mem->csr->mode == PRIVILEGE_MODE_M) {
    // The satp register is considered active when the effective privilege mode is S-mode or U-mode.
    // Executions of the address-translation algorithm may only begin using a given value of satp when satp is active.
    return addr;
  } else {
    // search TLB first
    unsigned paddr = 0;
    unsigned tlb_hit = 0;
    for (unsigned i = 0; i < mem->tlbs; i++) {
      if (mem->tlb_key[i] == (addr & 0xfffff000)) {
        paddr = mem->tlb_val[i] | (addr & 0x00000fff);
        tlb_hit = 1;
        break;
      }
    }
    if (tlb_hit == 0) {
      // hardware page walking
      unsigned vpn1 = (addr >> 22) & 0x000003ff;
      unsigned vpn0 = (addr >> 12) & 0x000003ff;
      unsigned pte1 = 0, pte0 = 0;
      unsigned pte1_addr = 0, pte0_addr = 0;
      // level 1
      pte1_addr = mem->vmrppn + (vpn1 << 2); // word
      unsigned *pte1_p = (unsigned *)memory_get_page(mem, pte1_addr - MEMORY_BASE_ADDR_RAM);
      pte1 = pte1_p[(pte1_addr & 0x00000fff) >> 2];
      // level 0
      pte0_addr = ((pte1 >> 10) << 12) + (vpn0 << 2);
      unsigned *pte0_p = (unsigned *)memory_get_page(mem, pte0_addr - MEMORY_BASE_ADDR_RAM);
      pte0 = pte0_p[(pte0_addr & 0x00000fff) >> 2];
      paddr = ((pte0 & 0xfff00000) << 2) | ((pte0 & 0x000ffc00) << 2) | (addr & 0x00000fff);
      // register to TLB
      mem->tlbs++;
      mem->tlb_val = (unsigned *)realloc(mem->tlb_val, mem->tlbs * sizeof(unsigned));
      mem->tlb_key = (unsigned *)realloc(mem->tlb_key, mem->tlbs * sizeof(unsigned));
      mem->tlb_key[mem->tlbs - 1] = addr & 0xfffff000;
      mem->tlb_val[mem->tlbs - 1] = paddr & 0xfffff000;
#if 0
      if (addr >= 0x10000000 && addr < 0x10000400) {
        printf("VADDR: %08x -> PADDR: %08x\n", addr, paddr);
        printf("pte1: %08x, pte1_addr: %08x, vpn1: %08x\n", pte1, pte1_addr, vpn1);
        printf("pte0: %08x, pte0_addr: %08x, vpn0: %08x\n", pte0, pte0_addr, vpn0);
      }
#endif
    }
    return paddr;
  }
}

static unsigned memory_get_memory_type(memory_t *mem, unsigned addr) {
  unsigned base = (addr & 0xff000000);
  if (base == MEMORY_BASE_ADDR_UART) {
    if (addr < MEMORY_BASE_ADDR_DISK) {
      return MEMORY_UART;
    } else {
      return MEMORY_DISK;
    }
  } else if (base == MEMORY_BASE_ADDR_ACLINT) {
    return MEMORY_ACLINT;
  } else if (base == MEMORY_BASE_ADDR_PLIC) {
    return MEMORY_PLIC;
  } else {
    if (base >= MEMORY_BASE_ADDR_RAM) {
      return MEMORY_RAM;
    } else {
      return MEMORY_NONE;
    }
  }
}

static char memory_aclint_load(memory_t *mem, unsigned addr) {
  char value;
  uint64_t byte_offset = addr % 8;
  uint64_t value64 = 0;
  if (addr < 0x0000BFF8) {
    value64 = csr_get_timecmp(mem->csr);
  } else if (addr <= 0x0000BFFF) {
    value64 =
      ((uint64_t)csr_csrr(mem->csr, CSR_ADDR_U_TIMEH) << 32) |
      ((uint64_t)csr_csrr(mem->csr, CSR_ADDR_U_TIME));
  } else {
    csr_set_tval(mem->csr, addr);
    csr_trap(mem->csr, TRAP_CODE_LOAD_ACCESS_FAULT);
  }
  value = (value64 >> (8 * byte_offset));
  return value;
}

static void memory_aclint_store(memory_t *mem, unsigned addr, char value) {
  if (addr < 0x0000BFF8) {
    // hart 0 mtimecmp
    uint64_t byte_offset = addr % 8;
    uint64_t mask = (0x0FFL << (8 * byte_offset)) ^ 0xFFFFFFFFFFFFFFFF;
    uint64_t timecmp = csr_get_timecmp(mem->csr);
    timecmp = (timecmp & mask) | (((uint64_t)value << (8 * byte_offset)) & (0xFFL << (8 * byte_offset)));
    csr_set_timecmp(mem->csr, timecmp);
  } else if (addr <= 0x0000BFFF) {
    // mtime read only
  } else {
    csr_set_tval(mem->csr, addr);
    csr_trap(mem->csr, TRAP_CODE_STORE_ACCESS_FAULT);
  }
}

unsigned memory_load(memory_t *mem, unsigned addr, unsigned size, unsigned reserved) {
  unsigned paddr = memory_address_translation(mem, addr);
  unsigned value = 0;
  switch (memory_get_memory_type(mem, paddr)) {
  case MEMORY_UART:
    for (unsigned i = 0; i < size; i++) {
      value |= ((0x000000ff & uart_read(mem->uart, paddr + i - MEMORY_BASE_ADDR_UART)) << (8 * i));
    }
    break;
  case MEMORY_DISK:
    for (unsigned i = 0; i < size; i++) {
      value |= ((0x000000ff & disk_read(mem->disk, paddr + i - MEMORY_BASE_ADDR_DISK)) << (8 * i));
    }
    break;
  case MEMORY_RAM:
    {
      char *page = memory_get_page(mem, paddr - MEMORY_BASE_ADDR_RAM);
      for (unsigned i = 0; i < size; i++) {
        value |= ((0x000000ff & page[((paddr + i) & 0x00000fff)]) << (8 * i));
      }
      if (reserved) {
        mem->reserve[memory_get_block_id(mem, paddr - MEMORY_BASE_ADDR_RAM)] = 1;
      }
    }
    break;
  case MEMORY_ACLINT:
    for (unsigned i = 0; i < size; i++) {
      value |= ((0x000000ff & memory_aclint_load(mem, paddr + i - MEMORY_BASE_ADDR_ACLINT)) << (8 * i));
    }
    break;
  case MEMORY_PLIC:
    for (unsigned i = 0; i < size; i++) {
      value |= ((0x000000ff & plic_read(mem->plic, paddr + i - MEMORY_BASE_ADDR_PLIC)) << (8 * i));
    }
    break;
  default:
    csr_set_tval(mem->csr, addr);
    csr_trap(mem->csr, TRAP_CODE_LOAD_ACCESS_FAULT);
    break;
  }
  return value;
}

unsigned memory_store(memory_t *mem, unsigned addr, unsigned value, unsigned size, unsigned conditional) {
  unsigned ret = MEMORY_STORE_SUCCESS;
  unsigned paddr = memory_address_translation(mem, addr);
  switch (memory_get_memory_type(mem, paddr)) {
  case MEMORY_UART:
    for (unsigned i = 0; i < size; i++) {
      uart_write(mem->uart, paddr + i - MEMORY_BASE_ADDR_UART, (char)(value >> (i * 8)));
    }
    break;
  case MEMORY_DISK:
    for (unsigned i = 0; i < size; i++) {
      disk_write(mem->disk, paddr + i - MEMORY_BASE_ADDR_DISK, (char)(value >> (i * 8)));
    }
    break;
  case MEMORY_RAM:
    {
      if (!conditional || mem->reserve[memory_get_block_id(mem, paddr - MEMORY_BASE_ADDR_RAM)]) {
        char *page = memory_get_page(mem, paddr - MEMORY_BASE_ADDR_RAM);
        for (unsigned i = 0; i < size; i++) {
          page[(paddr + i) & 0x00000fff] = (char)(value >> (i * 8));
        }
      } else {
        ret = MEMORY_STORE_FAILURE;
      }
    }
    break;
  case MEMORY_ACLINT:
    for (unsigned i = 0; i < size; i++) {
      memory_aclint_store(mem, paddr + i - MEMORY_BASE_ADDR_ACLINT, (char)(value >> (i * 8)));
    }
    break;
  case MEMORY_PLIC:
    for (unsigned i = 0; i < size; i++) {
      plic_write(mem->plic, paddr + i - MEMORY_BASE_ADDR_PLIC, (char)(value >> (i * 8)));
    }
    break;
  default:
    csr_set_tval(mem->csr, addr);
    csr_trap(mem->csr, TRAP_CODE_STORE_ACCESS_FAULT);
    break;
  }
  return ret;
}

unsigned memory_load_reserved(memory_t *mem, unsigned addr) {
  unsigned ret;
  ret = memory_load(mem, addr, 4, 1);
  return ret;
}

unsigned memory_store_conditional(memory_t *mem, unsigned addr, unsigned value) {
  return memory_store(mem, addr, value, 4, 1);
}

void memory_atp_on(memory_t *mem, unsigned ppn) {
  mem->vmflag = 1;
  mem->vmrppn = ppn << 12;
  return;
}

void memory_atp_off(memory_t *mem) {
  mem->vmflag = 0;
  mem->vmrppn = 0;
  return;
}

void memory_tlb_clear(memory_t *mem) {
  mem->tlbs = 0;
  free(mem->tlb_key);
  free(mem->tlb_val);
  mem->tlb_key = NULL;
  mem->tlb_val = NULL;
}

void memory_fini(memory_t *mem) {
  free(mem->base);
  for (unsigned i = 0; i < mem->blocks; i++) {
    free(mem->block[i]);
  }
  free(mem->block);
  free(mem->reserve);
  uart_fini(mem->uart);
  free(mem->uart);
  disk_fini(mem->disk);
  free(mem->disk);
  plic_fini(mem->plic);
  free(mem->plic);
  free(mem->tlb_key);
  free(mem->tlb_val);
  return;
}
