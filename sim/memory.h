#ifndef MEMORY_H
#define MEMORY_H

#include <stdio.h>

struct uart_t;

typedef struct memory_t {
  unsigned blocks;
  unsigned *base;
  char **block;
  char *reserve;
  // MMIO
  struct uart_t *uart;
} memory_t;

void memory_init(memory_t *);
char memory_load(memory_t *, unsigned addr);
void memory_store(memory_t *,unsigned addr, char value);
void memory_fini(memory_t *);
// atomic
unsigned memory_load_reserved(memory_t *, unsigned addr);
unsigned memory_store_conditional(memory_t *, unsigned addr, unsigned value);

#endif
