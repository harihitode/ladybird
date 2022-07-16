#ifndef MEMORY_H
#define MEMORY_H

typedef struct memory_t {
  unsigned blocks;
  unsigned *base;
  char **block;
} memory_t;

void memory_init(memory_t *);
char memory_load(memory_t *, unsigned addr);
void memory_store(memory_t *,unsigned addr, char value);
void memory_fini(memory_t *);

#endif
