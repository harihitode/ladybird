#include "memory.h"
#include "mmio.h"
#include "plic.h"
#include "sim.h"
#include "csr.h"
#include "lsu.h"
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

char *memory_get_page(memory_t *mem, unsigned addr, unsigned is_write, int device_id) {
  unsigned bid = (addr - mem->ram_base) / mem->ram_block_size;
  if (bid >= RAM_SIZE / mem->ram_block_size) {
    fprintf(stderr, "RAM Exceeds, %08x\n", addr);
    return NULL;
  }
  if (mem->ram_block[bid] == NULL) {
    mem->ram_block[bid] = (char *)malloc(mem->ram_block_size * sizeof(char));
  }
  if (is_write && device_id == DEVICE_ID_DMA) {
    // invalidate core's cache line
    memory_dcache_invalidate(mem); // TODO address base
  }
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
    rom_str(new_rom, rom_ptr, size);
  } else {
    rom_mmap(new_rom, rom_ptr, 1); // 1: read only
  }
  new_rom->next = NULL;
  return;
}

unsigned memory_atomic_operation(memory_t *mem, unsigned aquire, unsigned release,
                                 unsigned (*op)(unsigned, unsigned),
                                 struct core_step_result *result) {
  if (aquire) memory_dcache_write_back(mem);
  result->exception_code = memory_load(mem, 4, result);
  result->m_data = op(result->rd_data, result->m_data);
  result->exception_code = memory_store(mem, 4, result);
  if (release) memory_dcache_write_back(mem);
  return result->exception_code;
}

unsigned memory_fence_instruction(memory_t *mem) {
  memory_icache_invalidate(mem);
  memory_dcache_invalidate(mem);
  return 0;
}

unsigned memory_fence(memory_t *mem, unsigned char predecessor, unsigned char succussor) {
  memory_dcache_write_back(mem);
  return 0;
}

unsigned memory_fence_tso(memory_t *mem) {
  memory_dcache_write_back(mem);
  return 0;
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
  char *line = cache_get_line_ptr(cache, addr, 0);
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
  char *line = cache_get_line_ptr(cache, addr, 1);
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
    unsigned code = tlb_get(mem->tlb, mem->vmrppn, vaddr, paddr, access_type, prv);
#if 0
    fprintf(stderr, "vaddr: %08x, paddr: %08x, code: %08x\n", vaddr, *paddr, code);
#endif
    return code;
  }
}

unsigned memory_load(memory_t *mem, unsigned len, struct core_step_result *result) {
  result->exception_code = memory_address_translation(mem, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_LOAD, result->prv);
  if (result->exception_code) {
    return result->exception_code;
  }
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  unsigned found = 0;
  if (result->m_paddr >= mem->ram_base && result->m_paddr < ram_max_addr) {
    // RAM
    result->rd_data = memory_ram_load(mem, result->m_paddr, len, mem->dcache);
    found = 1;
  } else {
    // search MMIO
    for (unsigned u = 0; u < MAX_MMIO; u++) {
      struct mmio_t *unit = mem->mmio_list[u];
      if (unit == NULL) continue;
      if (result->m_paddr >= unit->base && result->m_paddr < (unit->base + unit->size)) {
        for (unsigned i = 0; i < len; i++) {
          result->rd_data |= ((0x000000ff & unit->readb(unit, result->m_paddr + i)) << (8 * i));
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
        if (result->m_paddr >= p->base && (result->m_paddr + len) < p->base + p->size) {
          rom = p;
          break;
        }
      }
      if (rom) {
        for (unsigned i = 0; i < len; i++) {
          result->rd_data |= ((0x000000ff & rom->data[result->m_paddr + i - rom->base]) << (8 * i));
        }
        found = 1;
      }
    }
  }
  if (found == 0) {
    result->exception_code = TRAP_CODE_LOAD_ACCESS_FAULT;
  }
  return result->exception_code;
}

unsigned memory_store(memory_t *mem, unsigned len, struct core_step_result *result) {
  result->exception_code = memory_address_translation(mem, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_STORE, result->prv);

  if (result->exception_code != 0) {
    return result->exception_code;
  }
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  if (result->m_paddr >= mem->ram_base && result->m_paddr < ram_max_addr) {
    // RAM
    memory_ram_store(mem, result->m_paddr, result->m_data, len, mem->dcache);
  } else {
    // search MMIO
    for (unsigned u = 0; u < MAX_MMIO; u++) {
      struct mmio_t *unit = mem->mmio_list[u];
      if (unit == NULL) continue;
      if (result->m_paddr >= unit->base && result->m_paddr < (unit->base + unit->size)) {
        for (unsigned i = 0; i < len; i++) {
          unit->writeb(unit, result->m_paddr + i, (char)(result->m_data >> (i * 8)));
        }
        break;
      }
    }
  }
  return result->exception_code;
}

unsigned memory_load_reserved(memory_t *mem, unsigned aquire, struct core_step_result *result) {
  result->exception_code = memory_address_translation(mem, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_LOAD, result->prv);
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  if (result->exception_code) {
    return result->exception_code;
  }
  if (result->m_paddr >= mem->ram_base && result->m_paddr < ram_max_addr) {
    // RAM
    result->rd_data = memory_ram_load(mem, result->m_paddr, 4, mem->dcache);
    if (!result->exception_code) {
      cache_line_t *cline = cache_get_line(mem->dcache, result->m_paddr, CACHE_ACCESS_READ);
      cline->reserved = 1;
    }
  } else {
    result->exception_code = TRAP_CODE_LOAD_ACCESS_FAULT;
  }
  return result->exception_code;
}

unsigned memory_store_conditional(memory_t *mem, unsigned release, struct core_step_result *result) {
  result->exception_code = memory_address_translation(mem, result->m_vaddr, &result->m_paddr, ACCESS_TYPE_STORE, result->prv);
  unsigned long long ram_max_addr = mem->ram_base + mem->ram_size;
  if (result->exception_code) {
    return result->exception_code;
  }
  if (result->m_paddr >= mem->ram_base && result->m_paddr < ram_max_addr) {
    cache_line_t *cline = cache_get_line(mem->dcache, result->m_paddr, CACHE_ACCESS_WRITE);
    if (cline->reserved == 1) {
      memory_ram_store(mem, result->m_paddr, result->m_data, 4, mem->dcache);
      if (!result->exception_code) {
        cline->reserved = 0;
        result->rd_data = MEMORY_STORE_SUCCESS;
      }
    } else {
      result->rd_data = MEMORY_STORE_FAILURE;
    }
  } else {
    result->exception_code = TRAP_CODE_STORE_ACCESS_FAULT;
  }
  return result->exception_code;
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

unsigned memory_dma_send(memory_t *mem, unsigned pbase, int len, char *data) {
  if (len <= 0) {
    return 0;
  }
#if 0
  printf("DMA pbase %08x len %08x\n", pbase, len);
#endif
  int len_remain = len;

  unsigned dest_base;
  unsigned src_base;
  unsigned burst_len;
  char *page;

  dest_base = pbase;
  src_base = 0;
  burst_len = (((dest_base & RAM_PAGE_OFFS_MASK) + len) < RAM_PAGE_SIZE) ? len : RAM_PAGE_SIZE - (dest_base & RAM_PAGE_OFFS_MASK);

  while (len_remain > 0) {
    page = memory_get_page(mem, dest_base, BUS_ACCESS_WRITE, DEVICE_ID_DMA);
    memcpy(&page[dest_base & RAM_PAGE_OFFS_MASK], &data[src_base], burst_len);
#if 0
    printf("DMA page %08x doffs %08x soffs %08x len %08x\n",
           dest_base & (~RAM_PAGE_OFFS_MASK), dest_base & RAM_PAGE_OFFS_MASK,
           src_base, burst_len);
#endif
    len_remain -= burst_len;
    dest_base += burst_len;
    src_base += burst_len;
    burst_len = (len_remain >= RAM_PAGE_SIZE) ? RAM_PAGE_SIZE : len_remain;
  }
  return 0;
}

unsigned memory_dma_send_c(memory_t *mem, unsigned pbase, int len, char data) {
  if (len <= 0) {
    return 0;
  }
#if 0
  printf("DMA (char) pbase %08x len %08x\n", pbase, len);
#endif
  int len_remain = len;

  unsigned dest_base;
  unsigned src_base;
  unsigned burst_len;
  char *page;

  dest_base = pbase;
  src_base = 0;
  burst_len = (((dest_base & RAM_PAGE_OFFS_MASK) + len) < RAM_PAGE_SIZE) ? len : RAM_PAGE_SIZE - (dest_base & RAM_PAGE_OFFS_MASK);

  while (len_remain > 0) {
    page = memory_get_page(mem, dest_base, BUS_ACCESS_WRITE, DEVICE_ID_DMA);
    memset(&page[dest_base & RAM_PAGE_OFFS_MASK], data, burst_len);
#if 0
    printf("DMA page %08x doffs %08x soffs %08x len %08x\n",
           dest_base & (~RAM_PAGE_OFFS_MASK), dest_base & RAM_PAGE_OFFS_MASK,
           src_base, burst_len);
#endif
    len_remain -= burst_len;
    dest_base += burst_len;
    src_base += burst_len;
    burst_len = (len_remain >= RAM_PAGE_SIZE) ? RAM_PAGE_SIZE : len_remain;
  }
  return 0;
}

void memory_fini(memory_t *mem) {
  for (unsigned i = 0; i < mem->ram_blocks; i++) {
    free(mem->ram_block[i]);
  }
  free(mem->ram_block);
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
    mem->icache->line[i].state = CACHE_INVALID;
  }
}

void memory_dcache_invalidate(memory_t *mem) {
  for (unsigned i = 0; i < mem->dcache->line_size; i++) {
    cache_write_back(mem->dcache, i);
    mem->dcache->line[i].state = CACHE_INVALID;
  }
}

void memory_dcache_invalidate_line(memory_t *mem, unsigned paddr) {
  for (unsigned i = 0; i < mem->dcache->line_size; i++) {
    if (mem->dcache->line[i].state != CACHE_INVALID && mem->dcache->line[i].tag == (paddr & mem->dcache->tag_mask)) {
      mem->dcache->line[i].state = CACHE_INVALID;
    }
  }
}

void memory_dcache_write_back(memory_t *mem) {
  for (unsigned i = 0; i < mem->dcache->line_size; i++) {
    cache_write_back(mem->dcache, i);
  }
}

void rom_init(rom_t *rom) {
  rom->rom_type = MEMORY_ROM_TYPE_DEFAULT;
  rom->data = NULL;
}

void rom_str(rom_t *rom, const char *str, unsigned size) {
  rom->rom_type = MEMORY_ROM_TYPE_DEFAULT;
  rom->data = (char *)calloc(size, sizeof(char));
  memcpy(rom->data, str, size);
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
