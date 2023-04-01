#include "lsu.h"
#include "riscv.h"
#include "sim.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lsu_init(lsu_t *lsu, memory_t *mem) {
  lsu->vmflag = 0;
  lsu->vmrppn = 0;
  lsu->icache = (cache_t *)malloc(sizeof(cache_t));
  lsu->dcache = (cache_t *)malloc(sizeof(cache_t));
  lsu->tlb = (tlb_t *)malloc(sizeof(tlb_t));
  cache_init(lsu->icache, mem, 32, 128); // 32 byte/line, 128 entry
  cache_init(lsu->dcache, mem, 32, 256); // 32 byte/line, 256 entry
  tlb_init(lsu->tlb, mem, 64); // 64 entry
  lsu->mem = mem;
  mem->cache = lsu->dcache;
  mem->dcache = lsu->dcache;
  mem->lsu = lsu;
}

unsigned lsu_address_translation(lsu_t *lsu, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv) {
  if (lsu->vmflag == 0 || prv == PRIVILEGE_MODE_M) {
    // The satp register is considered active when the effective privilege mode is S-mode or U-mode.
    // Executions of the address-translation algorithm may only begin using a given value of satp when satp is active.
    *paddr = vaddr;
    return 0;
  } else {
    unsigned code = tlb_get(lsu->tlb, lsu->vmrppn, vaddr, paddr, access_type, prv);
#if 0
    fprintf(stderr, "vaddr: %08x, paddr: %08x, code: %08x\n", vaddr, *paddr, code);
#endif
    return code;
  }
}

static int is_cachable(unsigned addr) {
  if (addr >= 0x80000000 && addr <= 0xFFFFFFFF) {
    return 1;
  } else {
    return 0;
  }
}

unsigned lsu_load(lsu_t *lsu, unsigned len, struct core_step_result *result) {
  return memory_load(lsu->mem, len, result);
}

unsigned lsu_store(lsu_t *lsu, unsigned len, struct core_step_result *result) {
  return memory_store(lsu->mem, len, result);
}

unsigned lsu_load_reserved(lsu_t *lsu, unsigned aquire, struct core_step_result *result) {
  return memory_load_reserved(lsu->mem, aquire, result);
}

unsigned lsu_store_conditional(lsu_t *lsu, unsigned release, struct core_step_result *result) {
  return memory_store_conditional(lsu->mem, release, result);
}

unsigned lsu_atomic_operation(lsu_t *lsu, unsigned aquire, unsigned release,
                              unsigned (*op)(unsigned, unsigned),
                              struct core_step_result *result) {
  return memory_atomic_operation(lsu->mem, aquire, release, op, result);
}

unsigned lsu_fence_instruction(lsu_t *lsu) {
  return memory_fence_instruction(lsu->mem);
}

unsigned lsu_fence(lsu_t *lsu, unsigned char predecessor, unsigned char successor) {
  return memory_fence(lsu->mem, predecessor, successor);
}

unsigned lsu_fence_tso(lsu_t *lsu) {
  return memory_fence_tso(lsu->mem);
}

void lsu_icache_invalidate(lsu_t *lsu) {
  for (unsigned i = 0; i < lsu->icache->line_size; i++) {
    lsu->icache->line[i].state = CACHE_INVALID;
  }
}

void lsu_dcache_invalidate(lsu_t *lsu) {
  memory_dcache_invalidate(lsu->mem);
}

void lsu_dcache_invalidate_line(lsu_t *lsu, unsigned paddr) {
  memory_dcache_invalidate_line(lsu->mem, paddr);
}

void lsu_dcache_write_back(lsu_t *lsu) {
  memory_dcache_write_back(lsu->mem);
}

void lsu_atp_on(lsu_t *lsu, unsigned ppn) {
  lsu->vmflag = 1;
  lsu->vmrppn = ppn << 12;
  return;
}

unsigned lsu_atp_get(lsu_t *lsu) {
  return (lsu->vmflag << 31) | ((lsu->vmrppn >> 12) & 0x000fffff);
}

void lsu_atp_off(lsu_t *lsu) {
  lsu->vmflag = 0;
  lsu->vmrppn = 0;
  return;
}

void lsu_tlb_clear(lsu_t *lsu) {
  tlb_clear(lsu->tlb);
}

void lsu_fini(lsu_t *lsu) {
  cache_fini(lsu->dcache);
  free(lsu->dcache);
  cache_fini(lsu->icache);
  free(lsu->icache);
  tlb_fini(lsu->tlb);
  free(lsu->tlb);
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
  cache->hart_id = 0;
}

cache_line_t *cache_get_line(cache_t *cache, unsigned addr, int is_write) {
  unsigned index = (addr & cache->index_mask) / cache->line_len;
  unsigned tag = addr & cache->tag_mask;
  unsigned ppage_found = 0;
  cache->access_count++;
  if (cache->line[index].state != CACHE_INVALID) {
    if (cache->line[index].tag == tag) {
      // hit
      cache->hit_count++;
      ppage_found = 1;
    } else {
      unsigned block_base = (addr & (~cache->line_mask)) & (cache->mem->ram_block_size - 1);
      // write back
      cache_write_back(cache, index);
      // read memory
      char *page = memory_get_page(cache->mem, addr, is_write, cache->hart_id);
      if (page) {
        memcpy(cache->line[index].data, &page[block_base], cache->line_len);
        cache->line[index].state = CACHE_SHARED;
        cache->line[index].reserved = 0;
        cache->line[index].tag = tag;
        ppage_found = 1;
      }
    }
  } else {
    unsigned block_base = (addr & (~cache->line_mask)) & (cache->mem->ram_block_size - 1);
    // read memory
    char *page = memory_get_page(cache->mem, addr, is_write, cache->hart_id);
    if (page) {
      memcpy(cache->line[index].data, &page[block_base], cache->line_len);
      cache->line[index].state = CACHE_SHARED;
      cache->line[index].reserved = 0;
      cache->line[index].tag = tag;
      ppage_found = 1;
    }
  }
  if (is_write && ppage_found) {
    cache->line[index].state = CACHE_MODIFIED;
  }
  if (ppage_found) {
    return &cache->line[index];
  } else {
    return NULL;
  }
}

char *cache_get_line_ptr(cache_t *cache, unsigned addr, int is_write) {
  cache_line_t *line = cache_get_line(cache, addr, is_write);
  if (line != NULL) {
    return &(line->data[addr & cache->line_mask]);
  } else {
    return NULL;
  }
}

int cache_write_back(cache_t *cache, unsigned index) {
  if (cache->line[index].state == CACHE_MODIFIED) {
    unsigned victim_addr = cache->line[index].tag | index * cache->line_len;
    unsigned victim_block_base = victim_addr & (cache->mem->ram_block_size - 1);
    char *page = memory_get_page(cache->mem, victim_addr, 1, cache->hart_id);
    // write back
    memcpy(&page[victim_block_base], cache->line[index].data, cache->line_len);
    cache->line[index].state = CACHE_SHARED;
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
  tlb->hart_id = 0;
}

void tlb_clear(tlb_t *tlb) {
  for (unsigned i = 0; i < tlb->line_size; i++) {
    tlb->line[i].valid = 0;
    tlb->line[i].megapage = 0;
  }
}

void tlb_fini(tlb_t *tlb) {
  free(tlb->line);
}

static int page_check_leaf(unsigned pte) {
  if (pte & (PTE_X | PTE_W | PTE_R)) {
    return 1;
  } else {
    return 0;
  }
}

static int page_check_privilege(unsigned pte, unsigned access_type, unsigned prv) {
  int protect = (pte & PTE_V);
  // check protect
  if (page_check_leaf(pte)) {
    if (prv == PRIVILEGE_MODE_U)
      protect = (protect && (pte & PTE_U));
    protect = (protect && (pte & access_type));
  } else {
    protect &= 1;
  }
  return protect;
}

#define PAGE_SUCCESS 0
#define PAGE_ACCESS_ERROR 1
#define PAGE_PRIVILEGE_ERROR 2
#define PAGE_SUCCESS_MEGAPAGE 3

static unsigned page_walk(memory_t *mem, int hart_id, unsigned vaddr, unsigned pte_base, unsigned *pte, unsigned access_type, unsigned prv) {
  unsigned access_fault = 0;
  unsigned protect_fault = 0;
  unsigned current_pte = 0;
  int level = 1;
  for (level = 1; level >= 0; level--) {
    unsigned pte_id = ((vaddr >> ((2 + (10 * (level + 1))) & 0x0000001f)) & 0x000003ff); // word offset
    unsigned pte_addr = pte_base + (pte_id * PTE_SIZE);
    if (pte_addr < mem->ram_base || (pte_addr > mem->ram_base + mem->ram_size)) {
#if 0
      fprintf(stderr, "access fault pte%d: addr: %08x base: %08x id: %08x\n", level, pte_addr, pte_base, pte_id);
#endif
      access_fault = 1;
      break;
    }
    current_pte = ((unsigned *)memory_get_page(mem, pte_base, 0, hart_id))[pte_id];
    if (level == 0) {
      if (page_check_leaf(current_pte) && page_check_privilege(current_pte, access_type, prv)) {
        protect_fault = 0;
      } else {
        protect_fault = 1;
      }
    } else {
      if (page_check_privilege(current_pte, access_type, prv)) {
        protect_fault = 0;
        if (page_check_leaf(current_pte)) {
          break;
        }
      } else {
        protect_fault = 1;
      }
    }
    if (protect_fault) {
#if 0
      fprintf(stderr, "TLB privilege error VADDR: %08x level: %d PTE_BASE: %08x ID: %u PTE: %08x current privilege: %u\n",
              vaddr, level, pte_base, pte_id, current_pte, prv);
#endif
      break;
    }
    pte_base = ((current_pte >> 10) << 12);
  }
  *pte = current_pte;
  if (access_fault) {
    return PAGE_ACCESS_ERROR;
  } else if (protect_fault) {
    return PAGE_PRIVILEGE_ERROR;
  } else {
    if (level == 1) {
      return PAGE_SUCCESS_MEGAPAGE;
    } else {
      return PAGE_SUCCESS;
    }
  }
}

unsigned tlb_get(tlb_t *tlb, unsigned vmrppn, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv) {
  *paddr = 0;
  // search TLB first
  unsigned index = (vaddr >> 12) & tlb->index_mask;
  unsigned tag = vaddr & tlb->tag_mask;
  unsigned pte = 0;
  unsigned exception = 0;
  tlb->access_count++;
  if (tlb->line[index].valid && (tlb->line[index].tag == tag)) {
    tlb->hit_count++;
    pte = tlb->line[index].value;
    if (page_check_privilege(pte, access_type, prv)) {
      if (tlb->line[index].megapage) {
        *paddr = ((pte & 0xfff00000) << 2) | (vaddr & 0x003fffff);
      } else {
        *paddr = ((pte & 0xfffffc00) << 2) | (vaddr & 0x00000fff);
      }
    } else {
      switch (access_type) {
      case ACCESS_TYPE_INSTRUCTION:
        exception = TRAP_CODE_INSTRUCTION_PAGE_FAULT;
        break;
      case ACCESS_TYPE_LOAD:
        exception = TRAP_CODE_LOAD_PAGE_FAULT;
        break;
      default:
        exception = TRAP_CODE_STORE_PAGE_FAULT;
        break;
      }
    }
  } else {
    // hardware page walking
    unsigned pte_base = vmrppn;
    unsigned pw_result = PAGE_SUCCESS;
#if 0
    fprintf(stderr, "HW page walking for addr: %08x, pte_base: %08x\n", vaddr, pte_base);
#endif

    // Pagewalking
    pw_result = page_walk(tlb->mem, tlb->hart_id, vaddr, pte_base, &pte, access_type, prv);
    if (pw_result == PAGE_SUCCESS_MEGAPAGE) {
      *paddr = ((pte & 0xfff00000) << 2) | (vaddr & 0x003fffff);
    } else {
      *paddr = ((pte & 0xfff00000) << 2) | ((pte & 0x000ffc00) << 2) | (vaddr & 0x00000fff);
    }

    if ((pw_result == PAGE_SUCCESS) || (pw_result == PAGE_SUCCESS_MEGAPAGE)) {
      // register to TLB
      tlb->line[index].valid = 1;
      tlb->line[index].tag = tag;
      tlb->line[index].value = pte;
      if (pw_result == PAGE_SUCCESS_MEGAPAGE) {
        tlb->line[index].megapage = 1;
      } else {
        tlb->line[index].megapage = 0;
      }
    } else if (pw_result == PAGE_ACCESS_ERROR) {
      switch (access_type) {
      case ACCESS_TYPE_INSTRUCTION:
        exception = TRAP_CODE_INSTRUCTION_ACCESS_FAULT;
        break;
      case ACCESS_TYPE_LOAD:
        exception = TRAP_CODE_LOAD_ACCESS_FAULT;
        break;
      default:
        exception = TRAP_CODE_STORE_ACCESS_FAULT;
        break;
      }
    } else {
      switch (access_type) {
      case ACCESS_TYPE_INSTRUCTION:
        exception = TRAP_CODE_INSTRUCTION_PAGE_FAULT;
        break;
      case ACCESS_TYPE_LOAD:
        exception = TRAP_CODE_LOAD_PAGE_FAULT;
        break;
      default:
        exception = TRAP_CODE_STORE_PAGE_FAULT;
        break;
      }
    }
  }
  return exception;
}
