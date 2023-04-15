#include "memory.h"
#include "lsu.h"
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
  mem->rom_list = NULL;
  mem->mmio_list = (struct mmio_t **)calloc(MAX_MMIO, sizeof(struct mmio_t *));
  mem->num_cache = 0;
  mem->cache_list = NULL;
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
  for (unsigned i = 0; i < mem->num_cache; i++) {
    cache_t *cache = mem->cache_list[i];
    unsigned tag_mask = cache->tag_mask;
    unsigned index = (addr & cache->index_mask) / cache->line_len;
    if (is_write && device_id != mem->cache_list[i]->hart_id) {
      if (cache->line[index].tag == (addr & tag_mask)) {
        cache->line[index].state = CACHE_INVALID;
      }
    }
  }
  return mem->ram_block[bid];
}

void memory_access_broadcast(memory_t *mem, unsigned addr, int is_write, int device_id) {
  for (unsigned i = 0; i < mem->num_cache; i++) {
    if (device_id != mem->cache_list[i]->hart_id) {
      cache_t *cache = mem->cache_list[i];
      unsigned tag_mask = cache->tag_mask;
      unsigned index = (addr & cache->index_mask) / cache->line_len;
      if (cache->line[index].tag == (addr & tag_mask)) {
        // MSI Protocol
        if (is_write) {
          if (cache->line[index].state == CACHE_SHARED) {
            cache->line[index].state = CACHE_INVALID;
          } else if (cache->line[index].state == CACHE_MODIFIED) {
            cache_write_back(cache, index);
            cache->line[index].state = CACHE_INVALID;
          }
        } else {
          if (cache->line[index].state == CACHE_MODIFIED) {
            cache_write_back(cache, index);
          }
        }
      }
    }
  }
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

unsigned memory_load(memory_t *mem, unsigned len, struct core_step_result *result) {
  unsigned found = 0;
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
  if (found == 0) {
    result->exception_code = TRAP_CODE_LOAD_ACCESS_FAULT;
  }
  return result->exception_code;
}

unsigned memory_store(memory_t *mem, unsigned len, struct core_step_result *result) {
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
  return result->exception_code;
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
  unsigned burst_len;
  char *page;

  dest_base = pbase;
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
    burst_len = (len_remain >= RAM_PAGE_SIZE) ? RAM_PAGE_SIZE : len_remain;
  }
  return 0;
}

void memory_add_cache(memory_t *mem, cache_t *cache) {
  if (mem->cache_list) {
    mem->cache_list = (cache_t **)realloc(mem->cache_list, (mem->num_cache + 1) * sizeof(cache_t *));
  } else {
    mem->cache_list = (cache_t **)malloc(1 * sizeof(cache_t *));
  }
  mem->cache_list[mem->num_cache++] = cache;
}

void memory_fini(memory_t *mem) {
  for (unsigned i = 0; i < mem->ram_blocks; i++) {
    free(mem->ram_block[i]);
  }
  free(mem->ram_block);
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
  free(mem->cache_list);
  return;
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
