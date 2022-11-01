#include "memory.h"
#include "mmio.h"
#include "plic.h"
#include "sim.h"
#include "csr.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// SV32 page table
#define PTE_V (1 << 0)
#define PTE_R (1 << 1)
#define PTE_W (1 << 2)
#define PTE_X (1 << 3)
#define PTE_U (1 << 4)

#define ACCESS_TYPE_INSTRUCTION PTE_X
#define ACCESS_TYPE_LOAD PTE_R
#define ACCESS_TYPE_STORE PTE_W
#define MAX_MMIO 8

void memory_init(memory_t *mem, unsigned ram_base, unsigned ram_size, unsigned ram_block_size) {
  mem->ram_base = ram_base;
  mem->ram_size = ram_size;
  mem->ram_block_size = ram_block_size;
  mem->ram_blocks = ram_size / ram_block_size;
  mem->ram_block = (char **)calloc(mem->ram_blocks, sizeof(char *));
  mem->ram_reserve = (char *)calloc(mem->ram_blocks, sizeof(char));
  mem->csr = NULL;
  mem->vmflag = 0;
  mem->vmrppn = 0;
  mem->icache = (cache_t *)malloc(sizeof(cache_t));
  mem->dcache = (cache_t *)malloc(sizeof(cache_t));
  mem->inst_line = NULL;
  mem->inst_line_pc = 0;
  mem->tlb = (tlb_t *)malloc(sizeof(tlb_t));
  cache_init(mem->icache, mem, 32, 128); // 32 byte/line, 128 entry
  cache_init(mem->dcache, mem, 32, 256); // 32 byte/line, 256 entry
  tlb_init(mem->tlb, mem, 64); // 64 entry
  mem->rom_list = NULL;
  mem->mmio_list = (struct mmio_t **)calloc(MAX_MMIO, sizeof(struct mmio_t *));
}

char *memory_get_page(memory_t *mem, unsigned addr) {
  unsigned bid = addr / mem->ram_block_size;
  if (mem->ram_block[bid] == NULL) {
    mem->ram_block[bid] = (char *)malloc(mem->ram_block_size * sizeof(char));
  }
  mem->ram_reserve[bid] = 0; // expire
  return mem->ram_block[bid];
}

void memory_set_rom(memory_t *mem, unsigned base, unsigned size, char *rom_ptr) {
  struct rom_t *new_rom;
  if (!mem->rom_list) {
    mem->rom_list = (struct rom_t *)calloc(1, sizeof(struct rom_t));
    new_rom = mem->rom_list;
  } else {
    for (new_rom = mem->rom_list; new_rom->next != NULL; new_rom = new_rom->next) {}
    new_rom->next = (struct rom_t *)calloc(1, sizeof(struct rom_t));
    new_rom = new_rom->next;
  }
  new_rom->base = base;
  new_rom->size = size;
  new_rom->rom = rom_ptr;
  new_rom->next = NULL;
  return;
}

void memory_set_mmio(memory_t *mem, struct mmio_t *unit, unsigned base, unsigned size) {
  unsigned empty = 0;
  for (empty = 0; empty < MAX_MMIO; empty++) {
    if (mem->mmio_list[empty] == NULL) break;
  }
  if (mem->mmio_list[empty] != NULL) {
    fprintf(stderr, "exceeds max mmio: %d\n", MAX_MMIO);
  } else {
    mem->mmio_list[empty] = unit;
    unit->base = base;
    unit->size = size;
  }
}

static unsigned memory_ram_load(memory_t *mem, unsigned addr, unsigned size, unsigned reserved, cache_t *cache) {
  unsigned value = 0;
  char *line = cache_get(cache, addr, 0);
  switch (size) {
  case 1:
    value = (unsigned char)(line[0]);
    break;
  case 2:
    value = (unsigned short)(((unsigned short *)line)[0]);
    break;
  case 4:
    value = (unsigned)(((unsigned *)line)[0]);
    break;
  default:
    break;
  }
  if (reserved) {
    mem->ram_reserve[addr / mem->ram_block_size] = 1;
  }
  return value;
}

static unsigned memory_ram_store(memory_t *mem, unsigned addr, unsigned value, unsigned size, unsigned conditional, cache_t *cache) {
  unsigned ret = MEMORY_STORE_SUCCESS;
  if (!conditional || mem->ram_reserve[addr / mem->ram_block_size]) {
    char *line = cache_get(cache, addr, 1);
    switch (size) {
    case 1:
      line[0] = (unsigned char)value;
      break;
    case 2:
      ((unsigned short *)line)[0] = (unsigned short)value;
      break;
    case 4:
      ((unsigned *)line)[0] = value;
      break;
    default:
      break;
    }
  } else {
    ret = MEMORY_STORE_FAILURE;
  }
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

unsigned memory_load(memory_t *mem, unsigned addr, unsigned size, unsigned reserved) {
  unsigned paddr = memory_address_translation(mem, addr, ACCESS_TYPE_LOAD);
  if (mem->csr->exception) {
    printf("exception!\n");
    return 0;
  }
  unsigned value = 0;
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  unsigned found = 0;
  if (paddr >= mem->ram_base && paddr < ram_max_addr) {
    // RAM
    value = memory_ram_load(mem, paddr - mem->ram_base, size, reserved, mem->dcache);
    found = 1;
  } else {
    // search MMIO
    for (unsigned u = 0; u < MAX_MMIO; u++) {
      struct mmio_t *unit = mem->mmio_list[u];
      if (unit == NULL) continue;
      if (paddr >= unit->base && paddr < (unit->base + unit->size)) {
        for (unsigned i = 0; i < size; i++) {
          value |= ((0x000000ff & unit->readb(unit, paddr + i - unit->base)) << (8 * i));
        }
        found = 1;
        break;
      }
    }
    // search ROM
    if (found == 0) {
      struct rom_t *rom = NULL;
      // search ROM
      for (struct rom_t *p = mem->rom_list; p != NULL; p++) {
        if (paddr >= p->base && (paddr + size) < p->base + p->size) {
          printf("ROM LOAD: %08x\n", paddr);
          rom = p;
          break;
        }
      }
      if (rom) {
        for (unsigned i = 0; i < size; i++) {
          value |= ((0x000000ff & rom->rom[paddr + i - rom->base]) << (8 * i));
        }
        found = 1;
      }
    }
  }
  if (found == 0) {
    if (mem->csr->mode == PRIVILEGE_MODE_M) {
      csr_csrw(mem->csr, CSR_ADDR_M_TVAL, paddr);
    } else {
      csr_csrw(mem->csr, CSR_ADDR_S_TVAL, paddr);
    }
    csr_exception(mem->csr, TRAP_CODE_LOAD_ACCESS_FAULT);
  }
  return value;
}

unsigned memory_store(memory_t *mem, unsigned addr, unsigned value, unsigned size, unsigned conditional) {
  unsigned ret = MEMORY_STORE_FAILURE;
  unsigned paddr = memory_address_translation(mem, addr, ACCESS_TYPE_STORE);
  if (mem->csr->exception) {
    return MEMORY_STORE_FAILURE;
  }
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  if (paddr >= mem->ram_base && paddr < ram_max_addr) {
    // RAM
    ret = memory_ram_store(mem, paddr - mem->ram_base, value, size, conditional, mem->dcache);
  } else {
    // search MMIO
    for (unsigned u = 0; u < MAX_MMIO; u++) {
      struct mmio_t *unit = mem->mmio_list[u];
      if (unit == NULL) continue;
      if (paddr >= unit->base && paddr < (unit->base + unit->size)) {
        for (unsigned i = 0; i < size; i++) {
          unit->writeb(unit, paddr + i - unit->base, (char)(value >> (i * 8)));
        }
        ret = MEMORY_STORE_SUCCESS;
        break;
      }
    }
  }
  if (ret == MEMORY_STORE_FAILURE) {
    if (mem->csr->mode == PRIVILEGE_MODE_M) {
      csr_csrw(mem->csr, CSR_ADDR_M_TVAL, paddr);
    } else {
      csr_csrw(mem->csr, CSR_ADDR_S_TVAL, paddr);
    }
    csr_exception(mem->csr, TRAP_CODE_STORE_ACCESS_FAULT);
  }
  return ret;
}

unsigned memory_load_reserved(memory_t *mem, unsigned addr) {
  unsigned ret;
  ret = memory_load(mem, addr, 4, 1);
  return ret;
}

unsigned memory_load_instruction(memory_t *mem, unsigned addr) {
  unsigned inst = 0;
  if (mem->inst_line && (mem->inst_line_pc == (addr & ~(mem->icache->line_mask)))) {
    mem->icache->access_count++;
    mem->icache->hit_count++;
    inst = ((unsigned short *)(&mem->inst_line[addr & mem->icache->line_mask]))[0];
    if ((inst & 0x03) == 3) {
      if ((addr & mem->icache->line_mask) == (mem->icache->line_len - 2)) {
        unsigned paddr = memory_address_translation(mem, addr, ACCESS_TYPE_INSTRUCTION);
        mem->inst_line_pc = ((addr + 2) & ~(mem->icache->line_mask));
        mem->inst_line = cache_get(mem->icache, (paddr + 2 - mem->ram_base) & ~(mem->icache->line_mask), 0);
      }
      inst |= (((unsigned short *)(&mem->inst_line[(addr + 2) & mem->icache->line_mask]))[0] << 16);
    }
  } else {
    unsigned paddr = memory_address_translation(mem, addr, ACCESS_TYPE_INSTRUCTION);
    // pmp check
    if (!mem->csr->exception) {
      if (paddr >= mem->ram_base && paddr < mem->ram_base + mem->ram_size) {
        mem->inst_line_pc = (addr & ~(mem->icache->line_mask));
        mem->inst_line = cache_get(mem->icache, (paddr - mem->ram_base) & ~(mem->icache->line_mask), 0);
        inst = ((unsigned short *)(&mem->inst_line[paddr & mem->icache->line_mask]))[0];
        if ((inst & 0x03) == 3) {
          if ((paddr & mem->icache->line_mask) == (mem->icache->line_len - 2)) {
            mem->inst_line_pc = ((addr + 2) & ~(mem->icache->line_mask));
            mem->inst_line = cache_get(mem->icache, (paddr + 2 - mem->ram_base) & ~(mem->icache->line_mask), 0);
          }
          inst |= (((unsigned short *)(&mem->inst_line[(paddr + 2) & mem->icache->line_mask]))[0] << 16);
        }
      } else {
        if (mem->csr->mode == PRIVILEGE_MODE_M) {
          csr_csrw(mem->csr, CSR_ADDR_M_TVAL, paddr);
        } else {
          csr_csrw(mem->csr, CSR_ADDR_S_TVAL, paddr);
        }
        csr_exception(mem->csr, TRAP_CODE_INSTRUCTION_ACCESS_FAULT);
      }
    }
  }
  return inst;
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
  for (unsigned i = 0; i < mem->ram_blocks; i++) {
    free(mem->ram_block[i]);
  }
  free(mem->ram_block);
  free(mem->ram_reserve);
  cache_fini(mem->dcache);
  free(mem->dcache);
  cache_fini(mem->icache);
  free(mem->icache);
  tlb_fini(mem->tlb);
  free(mem->tlb);
  struct rom_t *rom_p = mem->rom_list;
  while (1) {
    if (rom_p == NULL) {
      break;
    }
    struct rom_t *rom_np = rom_p->next;
    free(rom_p);
    rom_p = rom_np;
  }
  free(mem->mmio_list);
  return;
}

void memory_icache_invalidate(memory_t *mem) {
  for (unsigned i = 0; i < mem->icache->line_size; i++) {
    mem->icache->line[i].valid = 0;
  }
  mem->inst_line = NULL;
}

void memory_dcache_write_back(memory_t *mem) {
  for (unsigned i = 0; i < mem->dcache->line_size; i++) {
    cache_write_back(mem->dcache, i);
  }
}

void cache_init(cache_t *cache, memory_t *mem, unsigned line_len, unsigned line_size) {
  cache->mem = mem;
  cache->access_count = 0;
  cache->hit_count = 0;
  cache->line_len = line_len;
  cache->line_size = line_size;
  cache->line_mask = line_len - 1;
  cache->index_mask = ((cache->line_size * cache->line_len) - 1) ^ cache->line_mask;
  cache->tag_mask = (0xffffffff ^ (cache->index_mask | cache->line_mask));
  cache->line = (cache_line_t *)calloc(line_size, sizeof(cache_line_t));
  for (unsigned i = 0; i < line_size; i++) {
    cache->line[i].data = (char *)malloc(line_len * sizeof(char));
  }
}

char *cache_get(cache_t *cache, unsigned addr, char write) {
  unsigned index = (addr & cache->index_mask) / cache->line_len;
  unsigned tag = addr & cache->tag_mask;
  char *line = NULL;
  cache->access_count++;
  if (cache->line[index].valid) {
    if (cache->line[index].tag == tag) {
      // hit
      cache->hit_count++;
    } else {
      unsigned block_base = (addr & (~cache->line_mask)) & (cache->mem->ram_block_size - 1);
      // write back
      cache_write_back(cache, index);
      // read memory
      char *page = memory_get_page(cache->mem, addr);
      memcpy(cache->line[index].data, &page[block_base], cache->line_len);
      cache->line[index].valid = 1;
      cache->line[index].tag = tag;
      cache->line[index].dirty = 0;
    }
  } else {
    unsigned block_base = (addr & (~cache->line_mask)) & (cache->mem->ram_block_size - 1);
    // read memory
    char *page = memory_get_page(cache->mem, addr);
    memcpy(cache->line[index].data, &page[block_base], cache->line_len);
    cache->line[index].valid = 1;
    cache->line[index].tag = tag;
    cache->line[index].dirty = 0;
  }
  cache->line[index].dirty |= write;
  line = &(cache->line[index].data[addr & cache->line_mask]);
  return line;
}

int cache_write_back(cache_t *cache, unsigned index) {
  if (cache->line[index].valid && cache->line[index].dirty) {
    unsigned victim_addr = cache->line[index].tag | index * cache->line_len;
    unsigned victim_block_id = victim_addr / cache->mem->ram_block_size;
    unsigned victim_block_base = victim_addr & (cache->mem->ram_block_size - 1);
    // write back
    memcpy(&cache->mem->ram_block[victim_block_id][victim_block_base], cache->line[index].data, cache->line_len);
    cache->line[index].dirty = 0;
    return 1;
  } else {
    return 0;
  }
}

void cache_fini(cache_t *cache) {
  for (unsigned i = 0; i < cache->line_size; i++) {
    free(cache->line[i].data);
  }
  free(cache->line);
  return;
}

void tlb_init(tlb_t *tlb, memory_t *mem, unsigned size) {
  tlb->mem = mem;
  tlb->line_size = size;
  tlb->index_mask = tlb->line_size - 1;
  tlb->tag_mask = (0xffffffff ^ tlb->index_mask) << 12;
  tlb->line = (tlb_line_t *)calloc(size, sizeof(tlb_line_t));
  tlb->access_count = 0;
  tlb->hit_count = 0;
}

void tlb_clear(tlb_t *tlb) {
  for (unsigned i = 0; i < tlb->line_size; i++) {
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
    protect = (protect && (pte & access_type));
  }
  return protect;
}

unsigned tlb_get(tlb_t *tlb, unsigned addr, unsigned access_type) {
  unsigned paddr = 0;
  // search TLB first
  unsigned index = (addr >> 12) & tlb->index_mask;
  unsigned tag = addr & tlb->tag_mask;
  unsigned pte = 0;
  tlb->access_count++;
  if (tlb->line[index].valid && (tlb->line[index].tag == tag)) {
    tlb->hit_count++;
    pte = tlb->line[index].value;
    if (tlb_check_privilege(tlb, pte, 0, access_type)) {
      paddr = ((pte & 0xfffffc00) << 2) | (addr & 0x00000fff);
    } else {
      switch (access_type) {
      case ACCESS_TYPE_INSTRUCTION:
        csr_exception(tlb->mem->csr, TRAP_CODE_INSTRUCTION_PAGE_FAULT);
        break;
      case ACCESS_TYPE_LOAD:
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
      if (pte_addr < tlb->mem->ram_base || (pte_addr > tlb->mem->ram_base + tlb->mem->ram_size)) {
#if 0
        fprintf(stderr, "access fault pte%d: %08x, addr: %08x\n", i, pte1_addr, addr);
#endif
        access_fault = 1;
        break;
      }
      pte = memory_ram_load(tlb->mem, pte_addr - tlb->mem->ram_base, 4, 0, tlb->mem->dcache);
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
        if (tlb->mem->csr->mode == PRIVILEGE_MODE_M) {
          csr_csrw(tlb->mem->csr, CSR_ADDR_M_TVAL, addr);
        } else {
          csr_csrw(tlb->mem->csr, CSR_ADDR_S_TVAL, addr);
        }
        switch (access_type) {
        case ACCESS_TYPE_INSTRUCTION:
          csr_exception(tlb->mem->csr, TRAP_CODE_INSTRUCTION_ACCESS_FAULT);
          break;
        case ACCESS_TYPE_LOAD:
          csr_exception(tlb->mem->csr, TRAP_CODE_LOAD_ACCESS_FAULT);
          break;
        default:
          csr_exception(tlb->mem->csr, TRAP_CODE_STORE_ACCESS_FAULT);
          break;
        }
      } else {
        switch (access_type) {
        case ACCESS_TYPE_INSTRUCTION:
          csr_exception(tlb->mem->csr, TRAP_CODE_INSTRUCTION_PAGE_FAULT);
          break;
        case ACCESS_TYPE_LOAD:
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
