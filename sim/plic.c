#include "plic.h"
#include "sim.h"
#include <stdio.h>

void plic_init (plic_t *plic) {
  return;
}

unsigned plic_read(plic_t *plic, unsigned addr) {
  printf("PLIC read: %08x\n", addr);
  return 0;
}

void plic_write(plic_t *plic, unsigned addr, unsigned value) {
  printf("PLIC write: %08x, %08x\n", addr, value);
  return;
}


void plic_fini(plic_t *plic) {
  return;
}
