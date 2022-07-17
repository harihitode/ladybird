#include "memory.h"
#include <stdio.h>
#include <stdlib.h>

#define MEMORY_RAM 0
#define MEMORY_UART 1

void memory_init(memory_t *mem) {
  mem->blocks = 0;
  mem->base = NULL;
  mem->block = NULL;
  mem->reserve = NULL;
  mem->fi = stdin;
  mem->fo = stdout;
}

static unsigned memory_get_memory_type(memory_t *mem, unsigned addr) {
  if ((addr & 0xfffff000) == 0x10000000) {
    return MEMORY_UART;
  } else {
    return MEMORY_RAM;
  }
}

static char memory_read_uart(memory_t *mem, unsigned addr) {
  switch (addr) {
  case 0x10000000: // RX Register
    return 'a';
  case 0x10000005: // Line Status Register
    return 0x21; // always ready
  default:
    return 0;
  }
}

static void memory_write_uart(memory_t *mem, unsigned addr, unsigned value) {
  switch (addr) {
  case 0x10000000:
    fputc(value, mem->fo);
  default:
    return;
  }
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

char memory_load(memory_t *mem, unsigned addr) {
  char value;
  switch (memory_get_memory_type(mem, addr)) {
  case MEMORY_UART:
    value = memory_read_uart(mem, addr);
    break;
  default:
    {
      unsigned bid = memory_get_block_id(mem, addr);
      char *block = mem->block[bid];
      mem->reserve[bid] = 0; // expire
      value = block[(addr & 0x00000fff)];
    }
    break;
  }
  return value;
}

void memory_store(memory_t *mem, unsigned addr, char value) {
  switch (memory_get_memory_type(mem, addr)) {
  case MEMORY_UART:
    memory_write_uart(mem, addr, value);
    break;
  default:
    {
      unsigned bid = memory_get_block_id(mem, addr);
      char *block = mem->block[bid];
      mem->reserve[bid] = 0; // expire
      block[(addr & 0x00000fff)] = value;
    }
    break;
  }
  return;
}

unsigned memory_load_reserved(memory_t *mem, unsigned addr) {
  unsigned ret;
  char b0, b1, b2, b3;
  b0 = memory_load(mem, addr + 0);
  b1 = memory_load(mem, addr + 1);
  b2 = memory_load(mem, addr + 2);
  b3 = memory_load(mem, addr + 3);
  ret =
    ((b3 << 24) & 0xff000000) | ((b2 << 16) & 0x00ff0000) |
    ((b1 <<  8) & 0x0000ff00) | ((b0 <<  0) & 0x000000ff);
  mem->reserve[memory_get_block_id(mem, addr)] = 1; // TODO: only valid for only 1 hart
  return ret;
}

unsigned memory_store_conditional(memory_t *mem, unsigned addr, unsigned value) {
  const unsigned success = 0;
  const unsigned failure = 1;
  unsigned result;
  result = (mem->reserve[memory_get_block_id(mem, addr)]) ? success : failure;
  if (result == success) {
    memory_store(mem, addr + 0, (char)((value >>  0) & 0x000000ff));
    memory_store(mem, addr + 1, (char)((value >>  8) & 0x000000ff));
    memory_store(mem, addr + 2, (char)((value >> 16) & 0x000000ff));
    memory_store(mem, addr + 3, (char)((value >> 24) & 0x000000ff));
  }
  return result;
}

void memory_fini(memory_t *mem) {
  free(mem->base);
  for (unsigned i = 0; i < mem->blocks; i++) {
    free(mem->block[i]);
  }
  free(mem->block);
  free(mem->reserve);
  return;
}
