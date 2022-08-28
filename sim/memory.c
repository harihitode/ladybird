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

#define ACCESS_TYPE_INSTRUCTION 0
#define ACCESS_TYPE_LOAD 1
#define ACCESS_TYPE_STORE 2

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
    if ((base >= MEMORY_BASE_ADDR_RAM) && (base < MEMORY_BASE_ADDR_RAM + (128 << 20))) {
      return MEMORY_RAM;
    } else {
      return MEMORY_NONE;
    }
  }
}

// SV32 page table
#define PTE_V (1 << 0)
#define PTE_R (1 << 1)
#define PTE_W (1 << 2)
#define PTE_X (1 << 3)
#define PTE_U (1 << 4)

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
  mem->icache = (cache_t *)malloc(sizeof(cache_t));
  mem->dcache = (cache_t *)malloc(sizeof(cache_t));
  mem->tlb = (tlb_t *)malloc(sizeof(tlb_t));
  cache_init(mem->icache, mem, 512);
  cache_init(mem->dcache, mem, 512);
  tlb_init(mem->tlb, mem, 512);
}

void memory_set_sim(memory_t *mem, struct sim_t *sim) {
  mem->csr = sim->csr;
  return;
}

static unsigned memory_get_block_id(memory_t *mem, unsigned addr) {
  unsigned block_id = 0;
  unsigned hit = 0;
  for (unsigned i = 0; i < mem->blocks; i++) {
    // search blocks allocated for the base addr
    // block size is 4KB
    if (mem->base[i] == (addr & 0xfffff000)) {
      block_id = i;
      hit = 1;
      break;
    }
  }
  if (hit == 0) {
    unsigned last_block = mem->blocks;
    mem->blocks++; // increment
    mem->base = (unsigned *)realloc(mem->base, mem->blocks * sizeof(unsigned));
    mem->block = (char **)realloc(mem->block, mem->blocks * sizeof(char *));
    mem->reserve = (char *)realloc(mem->reserve, mem->blocks * sizeof(char));
    mem->base[last_block] = (addr & 0xfffff000);
    mem->block[last_block] = (char *)malloc(0x00001000 * sizeof(char));
    mem->reserve[last_block] = 0;
    block_id = last_block;
  }
  return block_id;
}

char *memory_get_page(memory_t *mem, unsigned addr) {
  unsigned bid = memory_get_block_id(mem, addr);
  char *page = mem->block[bid];
  mem->reserve[bid] = 0; // expire
  return page;
}

static unsigned memory_ram_load(memory_t *mem, unsigned addr, unsigned size, unsigned reserved, cache_t *cache) {
  unsigned value = 0;
  unsigned block_id = cache_get(cache, addr);
  char *page = mem->block[block_id];
  for (unsigned i = 0; i < size; i++) {
    value |= ((0x000000ff & page[((addr + i) & 0x00000fff)]) << (8 * i));
  }
  if (reserved) {
    mem->reserve[block_id] = 1;
  }
  return value;
}

static unsigned memory_ram_store(memory_t *mem, unsigned addr, unsigned value, unsigned size, unsigned conditional, cache_t *cache) {
  unsigned ret = MEMORY_STORE_SUCCESS;
  unsigned block_id = cache_get(cache, addr);
  if (!conditional || mem->reserve[block_id]) {
    char *page = mem->block[block_id];
    for (unsigned i = 0; i < size; i++) {
      page[(addr + i) & 0x00000fff] = (char)(value >> (i * 8));
    }
  } else {
    ret = MEMORY_STORE_FAILURE;
  }
  mem->reserve[block_id] = 0;
  return ret;
}

unsigned memory_address_translation(memory_t *mem, unsigned addr, unsigned access_type) {
  if (mem->vmflag == 0 || mem->csr->mode == PRIVILEGE_MODE_M) {
    // The satp register is considered active when the effective privilege mode is S-mode or U-mode.
    // Executions of the address-translation algorithm may only begin using a given value of satp when satp is active.
    return addr;
  } else {
    return tlb_get(mem->tlb, addr, access_type);
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
  unsigned paddr = memory_address_translation(mem, addr, ACCESS_TYPE_LOAD);
  if (mem->csr->exception) {
    return 0;
  }
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
    value = memory_ram_load(mem, paddr - MEMORY_BASE_ADDR_RAM, size, reserved, mem->dcache);
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
    csr_set_tval(mem->csr, paddr);
    csr_trap(mem->csr, TRAP_CODE_LOAD_ACCESS_FAULT);
    break;
  }
  return value;
}

unsigned memory_store(memory_t *mem, unsigned addr, unsigned value, unsigned size, unsigned conditional) {
  unsigned ret = MEMORY_STORE_SUCCESS;
  unsigned paddr = memory_address_translation(mem, addr, ACCESS_TYPE_STORE);
  if (mem->csr->exception) {
    return MEMORY_STORE_FAILURE;
  }
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
    ret = memory_ram_store(mem, paddr - MEMORY_BASE_ADDR_RAM, value, size, conditional, mem->dcache);
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
    csr_set_tval(mem->csr, paddr);
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

unsigned memory_load_instruction(memory_t *mem, unsigned addr) {
  unsigned paddr = memory_address_translation(mem, addr, ACCESS_TYPE_INSTRUCTION);
  // pmp check
  if (mem->csr->exception) {
    return 0;
  } else {
    if (memory_get_memory_type(mem, paddr) == MEMORY_RAM) {
      return memory_ram_load(mem, paddr - MEMORY_BASE_ADDR_RAM, 4, 1, mem->icache);
    } else {
      csr_set_tval(mem->csr, paddr);
      csr_exception(mem->csr, TRAP_CODE_INSTRUCTION_ACCESS_FAULT);
      return 0;
    }
  }
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
  tlb_clear(mem->tlb);
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
  cache_fini(mem->dcache);
  free(mem->dcache);
  cache_fini(mem->icache);
  free(mem->icache);
  tlb_fini(mem->tlb);
  free(mem->tlb);
  return;
}

void cache_init(cache_t *cache, memory_t *mem, unsigned size) {
  cache->mem = mem;
  cache->access_count = 0;
  cache->hit_count = 0;
  cache->line_len = size;
  cache->line = (cache_line_t *)calloc(size, sizeof(cache_line_t));
}

unsigned cache_get(cache_t *cache, unsigned addr) {
  unsigned index = (addr >> 12) & 0x000000ff; // 512
  unsigned tag = addr & 0xfff00000;
  cache->access_count++;
  if (cache->line[index].valid && (cache->line[index].tag == tag)) {
    cache->hit_count++;
    return cache->line[index].id;
  } else {
    cache->line[index].valid = 1;
    cache->line[index].id = memory_get_block_id(cache->mem, addr);
    cache->line[index].tag = tag;
    return cache->line[index].id;
  }
}

void cache_clear(cache_t *cache) {
  for (unsigned i = 0; i < cache->line_len; i++) {
    cache->line[i].valid = 0;
  }
}

void cache_fini(cache_t *cache) {
  free(cache->line);
  return;
}

void tlb_init(tlb_t *tlb, memory_t *mem, unsigned size) {
  tlb->mem = mem;
  tlb->line_len = size;
  tlb->line = (tlb_line_t *)calloc(size, sizeof(tlb_line_t));
  tlb->access_count = 0;
  tlb->hit_count = 0;
}

void tlb_clear(tlb_t *tlb) {
  for (unsigned i = 0; i < tlb->line_len; i++) {
    tlb->line[i].valid = 0;
  }
}

void tlb_fini(tlb_t *tlb) {
  free(tlb->line);
}

static int tlb_check_privilege(tlb_t *tlb, unsigned pte, unsigned level, unsigned access_type) {
  int protect = (pte & PTE_V);
  // check protect
  if (level > 0) {
    // TODO: super page: currently level 1 should not be a leaf page table
    protect = (protect && (!(pte & PTE_R) && !(pte & PTE_W)));
  } else {
    // level 0 should be leaf page table
    if (tlb->mem->csr->mode == PRIVILEGE_MODE_U)
      protect = (protect && (pte & PTE_U));
    switch (access_type) {
    case ACCESS_TYPE_INSTRUCTION:
      protect = (protect && (pte & PTE_X));
      break;
    case ACCESS_TYPE_LOAD:
      protect = (protect && (pte & PTE_R));
      break;
    case ACCESS_TYPE_STORE:
      protect = (protect && (pte & PTE_W));
      break;
    default:
      break;
    }
  }
  return protect;
}

unsigned tlb_get(tlb_t *tlb, unsigned addr, unsigned access_type) {
  unsigned paddr = 0;
  // search TLB first
  unsigned index = (addr >> 12) & 0x000000ff; // 512
  unsigned tag = addr & 0xfff00000;
  unsigned pte = 0;
  tlb->access_count++;
  if (tlb->line[index].valid && (tlb->line[index].tag == tag)) {
    tlb->hit_count++;
    pte = tlb->line[index].value;
    if (tlb_check_privilege(tlb, pte, 0, access_type)) {
      paddr = ((pte & 0xfff00000) << 2) | ((pte & 0x000ffc00) << 2) | (addr & 0x00000fff);
    } else {
      switch (access_type) {
      case TRAP_CODE_INSTRUCTION_PAGE_FAULT:
        csr_exception(tlb->mem->csr, TRAP_CODE_INSTRUCTION_PAGE_FAULT);
        break;
      case TRAP_CODE_LOAD_PAGE_FAULT:
        csr_exception(tlb->mem->csr, TRAP_CODE_LOAD_PAGE_FAULT);
        break;
      default:
        csr_exception(tlb->mem->csr, TRAP_CODE_STORE_PAGE_FAULT);
        break;
      }
    }
  } else {
    // hardware page walking
    // level 1
    unsigned pte_base = tlb->mem->vmrppn;
    int protect = 0;
    int access_fault = 0;
    for (int i = 1; i >= 0; i--) {
      unsigned pte_offs = ((addr >> ((2 + (10 * (i + 1))) & 0x0000001f)) & 0x000003ff) << 2; // word offset
      unsigned pte_addr = pte_base + pte_offs;
      if (memory_get_memory_type(tlb->mem, pte_addr) != MEMORY_RAM) {
#if 0
        fprintf(stderr, "access fault pte%d: %08x, addr: %08x\n", i, pte1_addr, addr);
#endif
        access_fault = 1;
        break;
      }
      pte = memory_ram_load(tlb->mem, pte_addr - MEMORY_BASE_ADDR_RAM, 4, 0, tlb->mem->dcache);
      protect = tlb_check_privilege(tlb, pte, i, access_type);
      if (!protect) {
        break;
      }
      pte_base = ((pte >> 10) << 12);
    }

    if (protect && !access_fault) {
      // register to TLB
      tlb->line[index].valid = 1;
      tlb->line[index].tag = tag;
      tlb->line[index].value = pte;
      paddr = ((pte & 0xfff00000) << 2) | ((pte & 0x000ffc00) << 2) | (addr & 0x00000fff);
    } else {
      if (access_fault) {
        csr_set_tval(tlb->mem->csr, addr);
        switch (access_type) {
        case TRAP_CODE_INSTRUCTION_PAGE_FAULT:
          csr_exception(tlb->mem->csr, TRAP_CODE_INSTRUCTION_ACCESS_FAULT);
          break;
        case TRAP_CODE_LOAD_PAGE_FAULT:
          csr_exception(tlb->mem->csr, TRAP_CODE_LOAD_ACCESS_FAULT);
          break;
        default:
          csr_exception(tlb->mem->csr, TRAP_CODE_STORE_ACCESS_FAULT);
          break;
        }
      } else {
        switch (access_type) {
        case TRAP_CODE_INSTRUCTION_PAGE_FAULT:
          csr_exception(tlb->mem->csr, TRAP_CODE_INSTRUCTION_PAGE_FAULT);
          break;
        case TRAP_CODE_LOAD_PAGE_FAULT:
          csr_exception(tlb->mem->csr, TRAP_CODE_LOAD_PAGE_FAULT);
          break;
        default:
          csr_exception(tlb->mem->csr, TRAP_CODE_STORE_PAGE_FAULT);
          break;
        }
      }
    }
  }
  return paddr;
}
