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
  lsu->mem = mem;
  lsu->icache = (cache_t *)malloc(sizeof(cache_t));
  lsu->dcache = (cache_t *)malloc(sizeof(cache_t));
  lsu->tlb = (tlb_t *)malloc(sizeof(tlb_t));
  cache_init(lsu->icache, mem, 32, 128); // 32 byte/line, 128 entry
  cache_init(lsu->dcache, mem, 32, 256); // 32 byte/line, 256 entry
  tlb_init(lsu->tlb, mem, 64); // 64 entry
  memory_add_cache(lsu->mem, lsu->dcache);
  for (int i = 0; i < 64; i++) {
    lsu->pmpcfg[i] = 0;
    lsu->pmpaddr[i] = 0;
  }
}

unsigned lsu_address_translation(lsu_t *lsu, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv) {
  unsigned exception_code = 0;
  if (lsu->vmflag == 0 || prv == PRIVILEGE_MODE_M) {
    // The satp register is considered active when the effective privilege mode is S-mode or U-mode.
    // Executions of the address-translation algorithm may only begin using a given value of satp when satp is active.
    *paddr = vaddr;
  } else {
    exception_code = tlb_get(lsu->tlb, lsu->vmrppn, vaddr, paddr, access_type, prv);
#if 0
    fprintf(stderr, "vaddr: %08x, paddr: %08x, code: %08x\n", vaddr, *paddr, exception_code);
#endif
  }
#if PMP_FEATURE
  // PMP check
  if (exception_code == 0) {
    int success = 1;
    for (int i = 0; i < 64; i++) {
      unsigned char locked = lsu->pmpcfg[i] >> 7;
      unsigned char address_matching = (lsu->pmpcfg[i] >> 3) & 0x3;
      int is_match = 0;
      if (!locked && prv == PRIVILEGE_MODE_M) continue;

      if (address_matching == CSR_PMPCFG_A_OFF) {
        continue;
      } else if (address_matching == CSR_PMPCFG_A_TOR) {
        unsigned from_addr = (i == 0) ? 0 : (lsu->pmpaddr[i - 1] << 2);
        unsigned to_addr = lsu->pmpaddr[i] << 2;
        if (*paddr >= from_addr && *paddr < to_addr) {
          is_match = 1;
        }
      } else if (address_matching == CSR_PMPCFG_A_NA4) {
        if ((*paddr & 0xfffffffc) == (lsu->pmpaddr[i - 1] << 2)) {
          is_match = 1;
        }
      } else {
        const unsigned pmpaddr = lsu->pmpaddr[i];
        int first_zero = 0;
        for (; (((pmpaddr >> first_zero) & 0x1) == 1) && first_zero != XLEN; first_zero++) { }
        unsigned long long check_addr = ((unsigned long long)pmpaddr) << 2;
        unsigned long long align_power = 1 << (first_zero + 3);
        unsigned long long align_power_mask = ~(align_power - 1);
        if ((check_addr & align_power_mask) == (*paddr & align_power_mask)) {
#if 0
          printf("[%d] %d %08llx %08llx %08x pmpaddr %08x firstzero %d R[%d] W[%d] X[%d] L[%d]\n", i, address_matching, check_addr, align_power_mask, *paddr,
                 pmpaddr, first_zero,
                 (lsu->pmpcfg[i] >> 0) & 0x1,
                 (lsu->pmpcfg[i] >> 1) & 0x1,
                 (lsu->pmpcfg[i] >> 2) & 0x1, locked);
#endif
          is_match = 1;
        }
      }
      if (is_match) {
        if (access_type == ACCESS_TYPE_INSTRUCTION) {
          success = (lsu->pmpcfg[i] >> 2) & 0x1; // executable flag
        } else if (access_type == ACCESS_TYPE_LOAD) {
          success = (lsu->pmpcfg[i] >> 0) & 0x1; // readable flag
        } else {
          success = (lsu->pmpcfg[i] >> 1) & 0x1; // writable flag
        }
        break;
      }
    }
    if (success == 0) {
      if (access_type == ACCESS_TYPE_INSTRUCTION) {
        exception_code = TRAP_CODE_INSTRUCTION_ACCESS_FAULT;
      } else if (access_type == ACCESS_TYPE_LOAD) {
        exception_code = TRAP_CODE_LOAD_ACCESS_FAULT;
      } else {
        exception_code = TRAP_CODE_STORE_ACCESS_FAULT;
      }
    }
  }
#endif
  return exception_code;
}

static int is_cacheable(unsigned addr) {
  if (addr >= MEMORY_BASE_ADDR_RAM) {
    return 1;
  } else {
    return 0;
  }
}

unsigned lsu_load(lsu_t *lsu, unsigned len, struct core_step_result *result) {
  result->exception_code = lsu_address_translation(lsu, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_LOAD, result->prv);
  if (result->exception_code) {
    return result->exception_code;
  }
  if (is_cacheable(result->m_paddr)) {
    char *line = cache_get_line_ptr(lsu->dcache, result->m_vaddr, result->m_paddr, CACHE_ACCESS_READ);
    if (line != NULL) {
      switch (len) {
      case 1:
        result->rd_data = (unsigned char)(line[0]);
        break;
      case 2:
        result->rd_data = (unsigned short)(((unsigned short *)line)[0]);
        break;
      case 4:
        result->rd_data = (unsigned)(((unsigned *)line)[0]);
        break;
      default:
        break;
      }
    } else {
      result->exception_code = TRAP_CODE_LOAD_ACCESS_FAULT;
    }
  } else {
    result->exception_code = memory_load(lsu->mem, len, MEMORY_LOAD_DEFAULT, result);
  }
  return result->exception_code;
}

unsigned lsu_store(lsu_t *lsu, unsigned len, struct core_step_result *result) {
  result->exception_code = lsu_address_translation(lsu, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_STORE, result->prv);
  if (result->exception_code) {
    return result->exception_code;
  }
  if (is_cacheable(result->m_paddr)) {
    char *line = cache_get_line_ptr(lsu->dcache, result->m_vaddr, result->m_paddr, CACHE_ACCESS_WRITE);
    if (line != NULL) {
      switch (len) {
      case 1:
        line[0] = (unsigned char)result->m_data;
        break;
      case 2:
        ((unsigned short *)line)[0] = (unsigned short)result->m_data;
        break;
      case 4:
        ((unsigned *)line)[0] = result->m_data;
        break;
      default:
        break;
      }
    } else {
      result->exception_code = TRAP_CODE_STORE_ACCESS_FAULT;
    }
  } else {
    result->exception_code = memory_store(lsu->mem, len, MEMORY_STORE_DEFAULT, result);
  }
  return result->exception_code;
}

unsigned lsu_load_reserved(lsu_t *lsu, unsigned aquire, struct core_step_result *result) {
  result->exception_code = lsu_address_translation(lsu, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_LOAD, result->prv);
  if (result->exception_code) {
    return result->exception_code;
  }
  if (is_cacheable(result->m_paddr)) {
    if (aquire) lsu_dcache_write_back(lsu);
    cache_line_t *cline = cache_get_line(lsu->dcache, result->m_vaddr, result->m_paddr, CACHE_ACCESS_READ);
    result->rd_data = *((unsigned *)(&cline->data[result->m_paddr & lsu->dcache->line_mask]));
    cline->reserved = 1;
  } else {
    result->exception_code = TRAP_CODE_LOAD_ACCESS_FAULT;
  }
  return result->exception_code;
}

unsigned lsu_store_conditional(lsu_t *lsu, unsigned release, struct core_step_result *result) {
  result->exception_code = lsu_address_translation(lsu, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_STORE, result->prv);
  if (result->exception_code) {
    return result->exception_code;
  }
  if (is_cacheable(result->m_paddr)) {
    cache_line_t *cline = cache_get_line(lsu->dcache, result->m_vaddr, result->m_paddr, CACHE_ACCESS_WRITE);
    if (cline->reserved == 1) {
      *((unsigned *)(&cline->data[result->m_paddr & lsu->dcache->line_mask])) = result->m_data;
      if (!result->exception_code) {
        cline->reserved = 0;
        result->rd_data = MEMORY_STORE_SUCCESS;
      }
    } else {
      result->rd_data = MEMORY_STORE_FAILURE;
    }
    if (release) lsu_dcache_write_back(lsu);
  } else {
    result->exception_code = TRAP_CODE_STORE_ACCESS_FAULT;
  }
  return result->exception_code;
}

unsigned lsu_atomic_operation(lsu_t *lsu, unsigned aquire, unsigned release,
                              unsigned (*op)(unsigned, unsigned),
                              struct core_step_result *result) {
  if (aquire) lsu_dcache_write_back(lsu);
  result->exception_code = lsu_load(lsu, 4, result);
  result->m_data = op(result->rd_data, result->m_data);
  result->exception_code = lsu_store(lsu, 4, result);
  if (release) lsu_dcache_write_back(lsu);
  return result->exception_code;
}

unsigned lsu_fence_instruction(lsu_t *lsu) {
  lsu_icache_invalidate(lsu);
  lsu_dcache_invalidate(lsu);
  return 0;
}

unsigned lsu_fence(lsu_t *lsu, unsigned char predecessor, unsigned char successor) {
  // only supported full fence
  lsu_dcache_write_back(lsu);
  return 0;
}

unsigned lsu_fence_tso(lsu_t *lsu) {
  lsu_dcache_write_back(lsu);
  return 0;
}

unsigned lsu_sfence_vma(lsu_t *lsu) {
  lsu_icache_invalidate(lsu);
  lsu_dcache_write_back(lsu);
  lsu_tlb_clear(lsu);
  return 0;
}

void lsu_icache_invalidate(lsu_t *lsu) {
  for (unsigned i = 0; i < lsu->icache->line_size; i++) {
    lsu->icache->line[i].state = CACHE_INVALID;
  }
}

void lsu_dcache_invalidate(lsu_t *lsu) {
  for (unsigned i = 0; i < lsu->dcache->line_size; i++) {
    cache_write_back(lsu->dcache, i);
    lsu->dcache->line[i].state = CACHE_INVALID;
  }
}

void lsu_dcache_invalidate_line(lsu_t *lsu, unsigned paddr) {
  for (unsigned i = 0; i < lsu->dcache->line_size; i++) {
    if (lsu->dcache->line[i].state != CACHE_INVALID && lsu->dcache->line[i].tag == (paddr & lsu->dcache->tag_mask)) {
      lsu->dcache->line[i].state = CACHE_INVALID;
    }
  }
}

void lsu_dcache_write_back(lsu_t *lsu) {
  for (unsigned i = 0; i < lsu->dcache->line_size; i++) {
    cache_write_back(lsu->dcache, i);
  }
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
  cache->id = 0;
  cache->tag_mode = CACHE_TAG_MODE_PIPT;
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

cache_line_t *cache_get_line(cache_t *cache, unsigned vaddr, unsigned paddr, int is_write) {
  // supporting VIPT and PIPT. ordinary L1 cache is VIPT reducing latency,
  // but this simulator's implementation assumes PIPT as default
  unsigned index = (cache->tag_mode == CACHE_TAG_MODE_PIPT) ?
    (paddr & cache->index_mask) / cache->line_len :
    (vaddr & cache->index_mask) / cache->line_len;
  unsigned tag = paddr & cache->tag_mask;
  // broadcast to other cashe
  memory_cache_coherent(cache->mem, paddr, cache->line_len, is_write, cache->id);
  cache->access_count++;
  if (cache->line[index].state != CACHE_INVALID) {
    if (cache->line[index].tag == tag) {
      // hit
      cache->hit_count++;
    } else {
      // writeback to memory
      cache_write_back(cache, index);
      // read from memory
      memory_cpy_from(cache->mem, cache->id, cache->line[index].data, paddr & (~cache->line_mask), cache->line_len);
      cache->line[index].state = CACHE_SHARED;
      cache->line[index].reserved = 0;
      cache->line[index].tag = tag;
    }
  } else {
    // read from memory
    memory_cpy_from(cache->mem, cache->id, cache->line[index].data, paddr & (~cache->line_mask), cache->line_len);
    cache->line[index].state = CACHE_SHARED;
    cache->line[index].reserved = 0;
    cache->line[index].tag = tag;
  }
  if (is_write) {
    cache->line[index].state = CACHE_MODIFIED;
  }
  return &cache->line[index];
}

char *cache_get_line_ptr(cache_t *cache, unsigned vaddr, unsigned paddr, int is_write) {
  cache_line_t *line = cache_get_line(cache, vaddr, paddr, is_write);
  if (line != NULL) {
    return &(line->data[paddr & cache->line_mask]);
  } else {
    return NULL;
  }
}

int cache_write_back(cache_t *cache, unsigned index) {
  if (cache->line[index].state == CACHE_MODIFIED) {
    unsigned victim_addr = cache->line[index].tag | index * cache->line_len;
    memory_cpy_to(cache->mem, cache->id, victim_addr, cache->line[index].data, cache->line_len);
    // MSI Protocol - write back SHARED
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
  tlb->id = 0;
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

static unsigned tlb_page_walk(tlb_t *tlb, unsigned vaddr, unsigned pte_base, unsigned *pte, unsigned access_type, unsigned prv) {
  memory_t *mem = tlb->mem;
  unsigned access_fault = 0;
  unsigned protect_fault = 0;
  unsigned current_pte = 0;
  int level = 1;
  for (level = 1; level >= 0; level--) {
    unsigned pte_id = ((vaddr >> ((2 + (10 * (level + 1))) & 0x0000001f)) & 0x000003ff); // word offset
    unsigned pte_addr = pte_base + (pte_id * PTE_SIZE);
    if (pte_addr < MEMORY_BASE_ADDR_RAM || (pte_addr > MEMORY_BASE_ADDR_RAM + RAM_SIZE)) {
#if 0
      fprintf(stderr, "access fault pte%d: addr: %08x base: %08x id: %08x\n", level, pte_addr, pte_base, pte_id);
#endif
      access_fault = 1;
      break;
    }
    memory_cpy_from(mem, tlb->id, (char *)&current_pte, pte_base + 4 * pte_id, 4);
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
    pw_result = tlb_page_walk(tlb, vaddr, pte_base, &pte, access_type, prv);
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
