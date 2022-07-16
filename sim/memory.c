#include "memory.h"
#include <stdio.h>
#include <stdlib.h>

void memory_init(memory_t *mem) {
  mem->blocks = 0;
  mem->base = NULL;
  mem->block = NULL;
}

static char *memory_get_block(memory_t *mem, unsigned addr) {
  for (unsigned i = 0; i < mem->blocks; i++) {
    // search blocks allocated for the base addr
    // block size is 4KB
    if (mem->base[i] == (addr & 0xfffff000)) {
      return mem->block[i];
    }
  }
  unsigned last_block = mem->blocks;
  mem->blocks++; // increment
  mem->base = (unsigned *)realloc(mem->base, mem->blocks * sizeof(unsigned));
  mem->block = (char **)realloc(mem->block, mem->blocks * sizeof(char *));
  mem->base[last_block] = (addr & 0xfffff000);
  mem->block[last_block] = (char *)malloc(0x00001000 * sizeof(char));
  return mem->block[last_block];
}

char memory_load(memory_t *mem, unsigned addr) {
  char *block = memory_get_block(mem, addr);
  return block[(addr & 0x00000fff)];
}

void memory_store(memory_t *mem, unsigned addr, char value) {
  char *block = memory_get_block(mem, addr);
  block[(addr & 0x00000fff)] = value;
  return;
}

void memory_fini(memory_t *mem) {
  free(mem->base);
  for (unsigned i = 0; i < mem->blocks; i++) {
    free(mem->block[i]);
  }
  free(mem->block);
  return;
}
