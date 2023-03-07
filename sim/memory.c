#include "memory.h"
#include "mmio.h"
#include "plic.h"
#include "sim.h"
#include "csr.h"
#include "riscv.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define SUPPORT_MEGAPAGE
#define MAX_MMIO 8

void memory_init(memory_t *mem, unsigned ram_base, unsigned ram_size, unsigned ram_block_size) {
  mem->ram_base = ram_base;
  mem->ram_size = ram_size;
  mem->ram_block_size = ram_block_size;
  mem->ram_blocks = ram_size / ram_block_size;
  mem->ram_block = (char **)calloc(mem->ram_blocks, sizeof(char *));
  mem->ram_reserve = (char *)calloc(mem->ram_blocks, sizeof(char));
  mem->vmflag = 0;
  mem->vmrppn = 0;
  mem->icache = (cache_t *)malloc(sizeof(cache_t));
  mem->dcache = (cache_t *)malloc(sizeof(cache_t));
  mem->tlb = (tlb_t *)malloc(sizeof(tlb_t));
  cache_init(mem->icache, mem, 32, 128); // 32 byte/line, 128 entry
  cache_init(mem->dcache, mem, 32, 256); // 32 byte/line, 256 entry
  tlb_init(mem->tlb, mem, 64); // 64 entry
  mem->rom_list = NULL;
  mem->mmio_list = (struct mmio_t **)calloc(MAX_MMIO, sizeof(struct mmio_t *));
}

char *memory_get_page(memory_t *mem, unsigned addr) {
  unsigned bid = (addr - mem->ram_base) / mem->ram_block_size;
  if (mem->ram_block[bid] == NULL) {
    mem->ram_block[bid] = (char *)malloc(mem->ram_block_size * sizeof(char));
  }
  mem->ram_reserve[bid] = 0; // expire
  return mem->ram_block[bid];
}

void memory_set_rom(memory_t *mem, const char *rom_ptr, unsigned base, unsigned size, unsigned type) {
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
  if (type == MEMORY_ROM_TYPE_DEFAULT) {
    rom_str(new_rom, rom_ptr);
  } else {
    rom_mmap(new_rom, rom_ptr, 1); // 1: read only
  }
  new_rom->next = NULL;
  return;
}

void memory_set_mmio(memory_t *mem, struct mmio_t *unit, unsigned base) {
  unsigned empty = 0;
  for (empty = 0; empty < MAX_MMIO; empty++) {
    if (mem->mmio_list[empty] == NULL) break;
  }
  if (mem->mmio_list[empty] != NULL) {
    fprintf(stderr, "exceeds max mmio: %d\n", MAX_MMIO);
  } else {
    mem->mmio_list[empty] = unit;
    unit->base = base;
  }
}

static unsigned memory_ram_load(memory_t *mem, unsigned addr, unsigned size, cache_t *cache) {
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
  return value;
}

static void memory_ram_store(memory_t *mem, unsigned addr, unsigned value, unsigned size, cache_t *cache) {
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
  return;
}

unsigned memory_address_translation(memory_t *mem, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv) {
  if (mem->vmflag == 0 || prv == PRIVILEGE_MODE_M) {
    // The satp register is considered active when the effective privilege mode is S-mode or U-mode.
    // Executions of the address-translation algorithm may only begin using a given value of satp when satp is active.
    *paddr = vaddr;
    return 0;
  } else {
    return tlb_get(mem->tlb, vaddr, paddr, access_type, prv);
  }
}

unsigned memory_load(memory_t *mem, unsigned addr, unsigned *value, unsigned size, unsigned prv) {
  *value = 0;
  unsigned paddr = 0;
  unsigned exception = memory_address_translation(mem, addr, &paddr, ACCESS_TYPE_LOAD, prv);
  if (exception) {
    return exception;
  }
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  unsigned found = 0;
  if (paddr >= mem->ram_base && paddr < ram_max_addr) {
    // RAM
    *value = memory_ram_load(mem, paddr, size, mem->dcache);
    found = 1;
  } else {
    // search MMIO
    for (unsigned u = 0; u < MAX_MMIO; u++) {
      struct mmio_t *unit = mem->mmio_list[u];
      if (unit == NULL) continue;
      if (paddr >= unit->base && paddr < (unit->base + unit->size)) {
        for (unsigned i = 0; i < size; i++) {
          *value |= ((0x000000ff & unit->readb(unit, paddr + i)) << (8 * i));
        }
        found = 1;
        break;
      }
    }
    // search ROM
    if (found == 0 && mem->rom_list) {
      struct rom_t *rom = NULL;
      // search ROM
      for (struct rom_t *p = mem->rom_list; p != NULL; p = p->next) {
        if (paddr >= p->base && (paddr + size) < p->base + p->size) {
          rom = p;
          break;
        }
      }
      if (rom) {
        for (unsigned i = 0; i < size; i++) {
          *value |= ((0x000000ff & rom->data[paddr + i - rom->base]) << (8 * i));
        }
        found = 1;
      }
    }
  }
  if (found == 0) {
    exception = TRAP_CODE_LOAD_ACCESS_FAULT;
  }
  return exception;
}

unsigned memory_store(memory_t *mem, unsigned addr, unsigned value, unsigned size, unsigned prv) {
  unsigned paddr = 0;
  unsigned exception = memory_address_translation(mem, addr, &paddr, ACCESS_TYPE_STORE, prv);
  if (exception != 0) {
    return exception;
  }
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  if (paddr >= mem->ram_base && paddr < ram_max_addr) {
    // RAM
    memory_ram_store(mem, paddr, value, size, mem->dcache);
  } else {
    // search MMIO
    for (unsigned u = 0; u < MAX_MMIO; u++) {
      struct mmio_t *unit = mem->mmio_list[u];
      if (unit == NULL) continue;
      if (paddr >= unit->base && paddr < (unit->base + unit->size)) {
        for (unsigned i = 0; i < size; i++) {
          unit->writeb(unit, paddr + i, (char)(value >> (i * 8)));
        }
        break;
      }
    }
  }
  return exception;
}

static void memory_ram_set_reserve(memory_t *mem, unsigned addr) {
  mem->ram_reserve[(addr - mem->ram_base) / mem->ram_block_size] = 1;
}

static void memory_ram_rst_reserve(memory_t *mem, unsigned addr) {
  mem->ram_reserve[(addr - mem->ram_base) / mem->ram_block_size] = 0;
}

static char memory_ram_get_reserve(memory_t *mem, unsigned addr) {
  return mem->ram_reserve[(addr - mem->ram_base) / mem->ram_block_size];
}

unsigned memory_load_reserved(memory_t *mem, unsigned addr, unsigned *value, unsigned prv) {
  unsigned paddr = 0;
  unsigned exception = memory_address_translation(mem, addr, &paddr, ACCESS_TYPE_LOAD, prv);
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  if (exception) {
    return exception;
  }
  if (paddr >= mem->ram_base && paddr < ram_max_addr) {
    // RAM
    *value = memory_ram_load(mem, paddr, 4, mem->dcache);
    memory_ram_set_reserve(mem, paddr);
    return 0;
  } else {
    return TRAP_CODE_LOAD_ACCESS_FAULT;
  }
}

unsigned memory_load_instruction(memory_t *mem, unsigned addr, unsigned *inst, unsigned prv) {
  unsigned paddr;
  unsigned exception = 0;
  char *inst_line;
  exception = memory_address_translation(mem, addr, &paddr, ACCESS_TYPE_INSTRUCTION, prv);
  // pmp check
  if (exception == 0) {
    if (paddr >= mem->ram_base && paddr < mem->ram_base + mem->ram_size) {
      inst_line = cache_get(mem->icache, paddr & ~(mem->icache->line_mask), 0);
      *inst = ((unsigned short *)(&inst_line[paddr & mem->icache->line_mask]))[0];
      if ((*inst & 0x03) == 3) {
        if ((paddr & mem->icache->line_mask) == (mem->icache->line_len - 2)) {
          inst_line = cache_get(mem->icache, (paddr + 2) & ~(mem->icache->line_mask), 0);
        }
        *inst |= (((unsigned short *)(&inst_line[(paddr + 2) & mem->icache->line_mask]))[0] << 16);
      }
    } else {
      exception = TRAP_CODE_INSTRUCTION_ACCESS_FAULT;
    }
  }
  return exception;
}

unsigned memory_store_conditional(memory_t *mem, unsigned addr, unsigned value, unsigned *success, unsigned prv) {
  unsigned paddr = 0;
  unsigned exception = memory_address_translation(mem, addr, &paddr, ACCESS_TYPE_STORE, prv);
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  if (exception) {
    return exception;
  }
  if (paddr >= mem->ram_base && paddr < ram_max_addr) {
    if (memory_ram_get_reserve(mem, paddr)) {
      memory_ram_store(mem, paddr, value, 4, mem->dcache);
      memory_ram_rst_reserve(mem, paddr);
      *success = MEMORY_STORE_SUCCESS;
    } else {
      *success = MEMORY_STORE_FAILURE;
    }
  } else {
    exception = TRAP_CODE_STORE_ACCESS_FAULT;
  }
  return exception;
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
}

void memory_dcache_invalidate(memory_t *mem, unsigned paddr) {
  for (unsigned i = 0; i < mem->dcache->line_size; i++) {
    if (mem->dcache->line[i].valid && mem->dcache->line[i].tag == (paddr & mem->dcache->tag_mask)) {
      mem->dcache->line[i].valid = 0;
    }
  }
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
    unsigned victim_block_id = (victim_addr - cache->mem->ram_base) / cache->mem->ram_block_size;
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
#define PAGE_NOTMEGAPAGE 3

#ifdef SUPPORT_MEGAPAGE
static unsigned megapage_walk(memory_t *mem, unsigned vaddr, unsigned pte_base, unsigned *pte, unsigned access_type, unsigned prv) {
  unsigned pte_offs = ((vaddr >> 22) & 0x000003ff) << 2; // word offset
  unsigned pte_addr = pte_base + pte_offs;
  unsigned entry = 0;
  if (pte_addr >= mem->ram_base && (pte_addr < mem->ram_base + mem->ram_size)) {
    entry = memory_ram_load(mem, pte_addr, 4, mem->dcache);
  }
  if (page_check_leaf(entry) && page_check_privilege(entry, access_type, prv)) {
    *pte = entry;
    return PAGE_SUCCESS;
  } else {
    return PAGE_NOTMEGAPAGE;
  }
}
#endif

static unsigned page_walk(memory_t *mem, unsigned vaddr, unsigned pte_base, unsigned *pte, unsigned access_type, unsigned prv) {
  unsigned access_fault = 0;
  unsigned protect_fault = 0;
  unsigned current_pte = 0;
  for (int i = 1; i >= 0; i--) {
    unsigned pte_offs = ((vaddr >> ((2 + (10 * (i + 1))) & 0x0000001f)) & 0x000003ff) << 2; // word offset
    unsigned pte_addr = pte_base + pte_offs;
    if (pte_addr < mem->ram_base || (pte_addr > mem->ram_base + mem->ram_size)) {
#if 0
      fprintf(stderr, "access fault pte%d: addr: %08x, offs: %08x\n", i, pte_addr, pte_offs);
#endif
      access_fault = 1;
      break;
    }
    current_pte = memory_ram_load(mem, pte_addr, 4, mem->dcache); // [TODO?] not to use dcache
    if (i == 0) {
      if (page_check_leaf(current_pte) && page_check_privilege(current_pte, access_type, prv)) {
        protect_fault = 0;
      } else {
        protect_fault = 1;
      }
    } else {
      if (page_check_privilege(current_pte, access_type, prv)) {
        protect_fault = 0;
      } else {
        protect_fault = 1;
      }
    }
    if (protect_fault) {
#if 0
      fprintf(stderr, "TLB privilege error (PTE_ADDR: %08x, PTE: %08x, current privilege: %u)\n", pte_addr, current_pte, prv);
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
    return PAGE_SUCCESS;
  }
}

unsigned tlb_get(tlb_t *tlb, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv) {
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
      *paddr = ((pte & 0xfffffc00) << 2) | (vaddr & 0x00000fff);
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
    unsigned pte_base = tlb->mem->vmrppn;
    unsigned pw_result = PAGE_SUCCESS;
#ifdef SUPPORT_MEGAPAGE
    pw_result = megapage_walk(tlb->mem, vaddr, pte_base, &pte, access_type, prv);
    if (pw_result != PAGE_SUCCESS)
#endif
      pw_result = page_walk(tlb->mem, vaddr, pte_base, &pte, access_type, prv);
    if (pw_result == PAGE_SUCCESS) {
      // register to TLB
      tlb->line[index].valid = 1;
      tlb->line[index].tag = tag;
      tlb->line[index].value = pte;
      *paddr = ((pte & 0xfff00000) << 2) | ((pte & 0x000ffc00) << 2) | (vaddr & 0x00000fff);
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

void rom_init(rom_t *rom) {
  rom->rom_type = MEMORY_ROM_TYPE_DEFAULT;
  rom->data = NULL;
}

void rom_str(rom_t *rom, const char *str) {
  rom->rom_type = MEMORY_ROM_TYPE_DEFAULT;
  rom->data = (char *)calloc(strlen(str) + 1, sizeof(char));
  strcpy(rom->data, str);
}

void rom_mmap(rom_t *rom, const char *img_path, int rom_mode) {
  int fd = 0;
  int open_flag = (rom_mode == 1) ? O_RDONLY : O_RDWR;
  int mmap_flag = (rom_mode == 1) ? MAP_PRIVATE : MAP_SHARED;
  if ((fd = open(img_path, open_flag)) == -1) {
    perror("rom file open");
    return;
  }
  stat(img_path, &rom->file_stat);
  rom->data = (char *)mmap(NULL, rom->file_stat.st_size, PROT_WRITE, mmap_flag, fd, 0);
  if (rom->data == MAP_FAILED) {
    perror("rom mmap");
    close(fd);
    return;
  }
  rom->rom_type = MEMORY_ROM_TYPE_MMAP;
}

void rom_fini(rom_t *rom) {
  if (rom->rom_type == MEMORY_ROM_TYPE_MMAP && rom->data) {
    munmap(rom->data, rom->file_stat.st_size);
  } else {
    free(rom->data);
  }
}
