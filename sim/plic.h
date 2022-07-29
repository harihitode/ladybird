#ifndef PLIC_H
#define PLIC_H

typedef struct plic_t {
  struct sim_t *sim;
} plic_t;

void plic_init(plic_t *);
unsigned plic_read(plic_t *, unsigned addr);
void plic_write(plic_t *, unsigned addr, unsigned value);
void plic_fini(plic_t *);

#endif
