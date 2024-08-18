#ifndef PLIC_H
#define PLIC_H

#include "memory.h"

typedef struct plic_t {
  struct mmio_t base;
  unsigned num_hart;
  struct mmio_t **peripherals;
  // for each prepherals
  unsigned *priorities;
  // for each hart context
  unsigned *interrupt_enable;
  unsigned *interrupt_threshold;
  unsigned *interrupt_complete;
  unsigned hart_rr;
} plic_t;

void plic_init(plic_t *);
void plic_add_hart(plic_t *);
unsigned plic_get_interrupt(plic_t *, unsigned context_id);
void plic_set_peripheral(plic_t *, struct mmio_t *, unsigned irq_no);
char plic_read(memory_target_t *, unsigned addr);
void plic_write(memory_target_t *, unsigned addr, char value);
void plic_fini(plic_t *);

typedef struct aclint_t {
  struct mmio_t base;
  unsigned num_hart;
  unsigned long long mtime;
  unsigned long long *mtimecmp;
  unsigned char *msip;
  unsigned char *ssip;
  unsigned cycle_count;
} aclint_t;

void aclint_init(aclint_t *);
void aclint_add_hart(aclint_t *);
char aclint_read(memory_target_t *, unsigned addr);
void aclint_write(memory_target_t *, unsigned addr, char value);
void aclint_cycle(aclint_t *);
unsigned long long aclint_get_mtimecmp(aclint_t *, int hart_id);
unsigned aclint_get_msip(aclint_t *, int hart_id);
unsigned aclint_get_ssip(aclint_t *, int hart_id);
void aclint_set_msip(aclint_t *, int hart_id, unsigned char val);
void aclint_set_ssip(aclint_t *, int hart_id, unsigned char val);
void aclint_fini(aclint_t *);

#endif
