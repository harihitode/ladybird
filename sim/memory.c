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

void memory_init(memory_t *mem) {
  mem->num_targets = 0;
  mem->targets = NULL;
  mem->num_cache = 0;
  mem->cache = NULL;
}

unsigned memory_load(memory_t *mem, unsigned len, unsigned reserved, struct core_step_result *result) {
  unsigned found = 0;
  for (unsigned u = 0; u < mem->num_targets; u++) {
    struct memory_target_t *unit = mem->targets[u];
    if ((result->m_paddr >= unit->base) && ((result->m_paddr + len) < (unit->base + unit->size))) {
      for (unsigned i = 0; i < len; i++) {
        result->rd_data |= ((0x000000ff & memory_target_readb(unit, result->hart_id, result->m_paddr + i)) << (8 * i));
      }
      if (reserved == MEMORY_LOAD_RESERVE) {
        memory_target_set_reserve_flag(unit, result->hart_id, result->m_paddr, len);
      }
      found = 1;
      break;
    }
  }
  if (found == 0) {
    result->exception_code = (reserved == MEMORY_LOAD_RESERVE) ? TRAP_CODE_AMO_ACCESS_FAULT : TRAP_CODE_LOAD_ACCESS_FAULT;
  }
  return result->exception_code;
}

unsigned memory_store(memory_t *mem, unsigned len, unsigned conditional, struct core_step_result *result) {
  unsigned found = 0;
  for (unsigned u = 0; u < mem->num_targets; u++) {
    struct memory_target_t *unit = mem->targets[u];
    if ((result->m_paddr >= unit->base) && ((result->m_paddr + len) < (unit->base + unit->size))) {
      if (conditional != MEMORY_STORE_CONDITIONAL ||
          memory_target_get_reserve_flag(unit, result->hart_id, result->m_paddr, len)) {
        // store success
        for (unsigned i = 0; i < len; i++) {
          memory_target_writeb(unit, result->hart_id, result->m_paddr + i, (char)(result->m_data >> (i * 8)));
        }
        result->rd_data = MEMORY_STORE_SUCCESS;
      } else {
        result->rd_data = MEMORY_STORE_FAILURE;
      }
      found = 1;
      break;
    }
  }
  if (found == 0) {
    result->exception_code = (conditional == MEMORY_STORE_CONDITIONAL) ? TRAP_CODE_AMO_ACCESS_FAULT : TRAP_CODE_STORE_ACCESS_FAULT;
  }
  return result->exception_code;
}

unsigned memory_cpy_to(memory_t *mem, int device_id, unsigned dst, const char *data, int len) {
  if (len <= 0) {
    return 0;
  }
#if 0
  printf("DMA pbase %08x len %08x\n", pbase, len);
#endif
  int len_remain = len;

  unsigned dst_base;
  unsigned src_base;
  unsigned burst_len;
  char *mem_ptr = NULL;

  dst_base = dst;
  src_base = 0;
  burst_len = (((dst_base & RAM_PAGE_OFFS_MASK) + len) < RAM_PAGE_SIZE) ? len : RAM_PAGE_SIZE - (dst_base & RAM_PAGE_OFFS_MASK);

  while (len_remain > 0) {
    for (unsigned u = 0; u < mem->num_targets; u++) {
      struct memory_target_t *unit = mem->targets[u];
      if ((dst_base >= unit->base) && (dst_base + burst_len < (unit->base + unit->size))) {
        mem_ptr = memory_target_get_ptr(unit, dst_base);
        if (mem_ptr) {
          memory_cache_coherent(mem, dst_base, burst_len, MEMORY_ACCESS_WRITE, device_id);
          memcpy(mem_ptr, &data[src_base], burst_len);
        } else {
          printf("fatal: DMA could not access this region %08x - %08x\n", dst_base, dst_base + burst_len);
        }
        break;
      }
    }
    len_remain -= burst_len;
    dst_base += burst_len;
    src_base += burst_len;
    burst_len = (len_remain >= RAM_PAGE_SIZE) ? RAM_PAGE_SIZE : len_remain;
  }
  return 0;
}

unsigned memory_cpy_from(memory_t *mem, int device_id, char *dst, unsigned src, int len) {
  if (len <= 0) {
    return 0;
  }
#if 0
  printf("DMA pbase %08x len %08x\n", pbase, len);
#endif
  int len_remain = len;

  unsigned dst_base;
  unsigned src_base;
  unsigned burst_len;
  const char *mem_ptr = NULL;

  dst_base = 0;
  src_base = src;
  burst_len = (((src_base & RAM_PAGE_OFFS_MASK) + len) < RAM_PAGE_SIZE) ? len : RAM_PAGE_SIZE - (src_base & RAM_PAGE_OFFS_MASK);

  while (len_remain > 0) {
    for (unsigned u = 0; u < mem->num_targets; u++) {
      struct memory_target_t *unit = mem->targets[u];
      if ((src_base >= unit->base) && (src_base + burst_len < (unit->base + unit->size))) {
        mem_ptr = memory_target_get_ptr(unit, src_base);
        if (mem_ptr) {
          memory_cache_coherent(mem, src_base, burst_len, MEMORY_ACCESS_READ, device_id);
          memcpy(&dst[dst_base], mem_ptr, burst_len);
        } else {
          printf("fatal: DMA could not access this region %08x - %08x\n", src_base, src_base + burst_len);
        }
        break;
      }
    }
    len_remain -= burst_len;
    dst_base += burst_len;
    src_base += burst_len;
    burst_len = (len_remain >= RAM_PAGE_SIZE) ? RAM_PAGE_SIZE : len_remain;
  }
  return 0;
}

unsigned memory_set(memory_t *mem, int device_id, unsigned dst, char data, int len) {
  if (len <= 0) {
    return 0;
  }
#if 0
  printf("DMA (char) pbase %08x len %08x\n", pbase, len);
#endif
  int len_remain = len;

  unsigned dst_base;
  unsigned burst_len;
  char *mem_ptr = NULL;

  dst_base = dst;
  burst_len = (((dst_base & RAM_PAGE_OFFS_MASK) + len) < RAM_PAGE_SIZE) ? len : RAM_PAGE_SIZE - (dst_base & RAM_PAGE_OFFS_MASK);

  while (len_remain > 0) {
    for (unsigned u = 0; u < mem->num_targets; u++) {
      struct memory_target_t *unit = mem->targets[u];
      if ((dst_base >= unit->base) && (dst_base + burst_len < (unit->base + unit->size))) {
        mem_ptr = memory_target_get_ptr(unit, dst_base);
        if (mem_ptr) {
          memory_cache_coherent(mem, dst_base, burst_len, MEMORY_ACCESS_WRITE, device_id);
          memset(mem_ptr, data, burst_len);
        } else {
          printf("fatal: DMA could not access this region %08x - %08x\n", dst_base, dst_base + burst_len);
        }
        break;
      }
    }
    len_remain -= burst_len;
    dst_base += burst_len;
    burst_len = (len_remain >= RAM_PAGE_SIZE) ? RAM_PAGE_SIZE : len_remain;
  }
  return 0;
}

void memory_add_target(memory_t *mem, struct memory_target_t *unit, unsigned base, unsigned size) {
  if (mem->targets) {
    mem->targets = (memory_target_t **)realloc(mem->targets, (mem->num_targets + 1) * sizeof(memory_target_t *));
  } else {
    mem->targets = (memory_target_t **)malloc(1 * sizeof(memory_target_t *));
  }
  mem->targets[mem->num_targets++] = unit;
  unit->base = base;
  unit->size = size;
  return;
}

void memory_add_cache(memory_t *mem, cache_t *cache) {
  if (mem->cache) {
    mem->cache = (cache_t **)realloc(mem->cache, (mem->num_cache + 1) * sizeof(cache_t *));
  } else {
    mem->cache = (cache_t **)malloc(1 * sizeof(cache_t *));
  }
  mem->cache[mem->num_cache++] = cache;
}

void memory_cache_coherent(memory_t *mem, unsigned addr, unsigned len, int is_write, int device_id) {
  for (unsigned i = 0; i < mem->num_cache; i++) {
    if (device_id != mem->cache[i]->id) {
      cache_t *cache = mem->cache[i];
      unsigned tag_mask = cache->tag_mask;
      for (unsigned j = 0; j < len; j++) {
        unsigned index = ((addr + j) & cache->index_mask) / cache->line_len;
        if (cache->line[index].tag == (addr & tag_mask)) {
          // MSI Protocol
          if (is_write) {
            if (cache->line[index].state == CACHE_SHARED) {
              // Other device want to write the line, then shard -> invalid
              cache->line[index].state = CACHE_INVALID;
            } else if (cache->line[index].state == CACHE_MODIFIED) {
              // Other device want to write the line, then writeback to invalid
              cache_write_back(cache, index);
              cache->line[index].state = CACHE_INVALID;
            }
          } else {
            if (cache->line[index].state == CACHE_MODIFIED) {
              // Other device want to read the line, then writeback to shared
              cache_write_back(cache, index);
              cache->line[index].state = CACHE_SHARED;
            }
          }
        }
      }
    }
  }
}

void memory_fini(memory_t *mem) {
  free(mem->targets);
  free(mem->cache);
  return;
}

void memory_target_init(memory_target_t *target, unsigned base, unsigned size,
                        char *(*get_ptr)(struct memory_target_t *target, unsigned addr),
                        char (*readb)(struct memory_target_t *target, unsigned addr),
                        void (*writeb)(struct memory_target_t *target, unsigned addr, char value)) {
  target->base = base;
  target->size = size;
  target->reserve_list_len = 0;
  target->reserve_list = NULL;
  target->get_ptr = get_ptr;
  target->readb = readb;
  target->writeb = writeb;
}

char memory_target_readb(struct memory_target_t *unit, unsigned transaction_id, unsigned addr) {
  // TODO reserved flag
  return unit->readb(unit, addr);
}

void memory_target_writeb(struct memory_target_t *unit, unsigned transaction_id, unsigned addr, char value) {
  // TODO reserved flag
  unit->writeb(unit, addr, value);
  return;
}

void memory_target_set_reserve_flag(struct memory_target_t *unit, unsigned transaction_id, unsigned addr, unsigned len) {
  // TODO
  return;
}

int memory_target_get_reserve_flag(struct memory_target_t *unit, unsigned transaction_id, unsigned addr, unsigned len) {
  // TODO
  return 0;
}

char *memory_target_get_ptr(struct memory_target_t *unit, unsigned addr) {
  if (unit->get_ptr) {
    return unit->get_ptr(unit, addr);
  } else {
    return NULL;
  }
}

void memory_target_fini(memory_target_t *target) {
  free(target->reserve_list);
}

void sram_init_with_char(sram_t *sram, const char clear, unsigned size) {
  sram->type = MEMORY_SRAM_TYPE_DEFAULT;
  sram->data = (char *)calloc(size, sizeof(char));
  memset(sram->data, clear, size);
  memory_target_init((memory_target_t *)sram, 0, size, NULL, sram_readb, sram_writeb);
}

void sram_init_with_str(sram_t *sram, const char *data, unsigned size) {
  sram->type = MEMORY_SRAM_TYPE_DEFAULT;
  sram->data = (char *)calloc(size, sizeof(char));
  memcpy(sram->data, data, size);
  memory_target_init((memory_target_t *)sram, 0, size, NULL, sram_readb, sram_writeb);
}

void sram_init_with_file(sram_t *sram, const char *img_path, int mode) {
  int fd = 0;
  int open_flag = (mode == MEMORY_SRAM_MODE_READ_ONLY) ? O_RDONLY : O_RDWR;
  int mmap_flag = (mode == MEMORY_SRAM_MODE_READ_ONLY) ? MAP_PRIVATE : MAP_SHARED;
  if ((fd = open(img_path, open_flag)) == -1) {
    perror("sram file open");
    return;
  }
  stat(img_path, &sram->file_stat);
  sram->data = (char *)mmap(NULL, sram->file_stat.st_size, PROT_WRITE, mmap_flag, fd, 0);
  if (sram->data == MAP_FAILED) {
    perror("sram mmap");
    close(fd);
    return;
  }
  sram->type = MEMORY_SRAM_TYPE_MMAP;
  memory_target_init((memory_target_t *)sram, 0, sram->file_stat.st_size, NULL, sram_readb, sram_writeb);
}

char sram_readb(struct memory_target_t *target, unsigned addr) {
  addr -= target->base;
  sram_t *sram = (sram_t *)target;
  if (addr < target->size) {
    return sram->data[addr];
  } else {
    return 0;
  }
}

void sram_writeb(struct memory_target_t *target, unsigned addr, char value) {
  addr -= target->base;
  sram_t *sram = (sram_t *)target;
  if (addr < target->size) {
    sram->data[addr] = value;
  }
  return;
}

void sram_fini(sram_t *sram) {
  if (sram->type == MEMORY_SRAM_TYPE_MMAP && sram->data) {
    munmap(sram->data, sram->file_stat.st_size);
  } else {
    free(sram->data);
  }
  memory_target_fini((memory_target_t *)sram);
}

void dram_init(dram_t *dram, unsigned size, unsigned block_size) {
  dram->block_size = block_size;
  dram->blocks = size / block_size;
  dram->block = (char **)calloc(dram->blocks, sizeof(char *));
  memory_target_init((memory_target_t *)dram, 0, size, dram_get_ptr, dram_readb, dram_writeb);
}

char *dram_get_ptr(struct memory_target_t *target, unsigned addr) {
  dram_t *dram = (dram_t *)target;
  unsigned bid = (addr - dram->base.base) / dram->block_size;
  unsigned offs = addr & RAM_PAGE_OFFS_MASK;
  if (bid >= RAM_SIZE / dram->block_size) {
    fprintf(stderr, "RAM Exceeds, %08x\n", addr);
    return NULL;
  }
  if (dram->block[bid] == NULL) {
    dram->block[bid] = (char *)malloc(dram->block_size * sizeof(char));
  }
  return &(dram->block[bid][offs]);
}

char dram_readb(struct memory_target_t *target, unsigned addr) {
  if (addr >= target->base && addr < target->base + target->size) {
    return *(dram_get_ptr(target, addr));
  } else {
    return 0;
  }
}

void dram_writeb(struct memory_target_t *target, unsigned addr, char value) {
  if (addr >= target->base && addr < target->base + target->size) {
    char *b = dram_get_ptr(target, addr);
    *b = value;
  }
  return;
}

void dram_fini(dram_t *dram) {
  for (unsigned i = 0; i < dram->blocks; i++) {
    free(dram->block[i]);
  }
  free(dram->block);
  memory_target_fini((memory_target_t *)dram);
}
