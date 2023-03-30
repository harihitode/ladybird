#ifndef PLIC_H
#define PLIC_H

#include "memory.h"

typedef struct plic_t {
  struct mmio_t base;
  struct mmio_t **peripherals;
  unsigned *priorities;
  unsigned m_interrupt_enable;
  unsigned s_interrupt_enable;
  unsigned m_interrupt_threshold;
  unsigned s_interrupt_threshold;
  unsigned m_interrupt_complete;
  unsigned s_interrupt_complete;
} plic_t;

void plic_init(plic_t *);
unsigned plic_get_interrupt(plic_t *, unsigned context);
void plic_set_peripheral(plic_t *, struct mmio_t *, unsigned irq_no);
char plic_read(struct mmio_t *, unsigned addr);
void plic_write(struct mmio_t *, unsigned addr, char value);
void plic_fini(plic_t *);

#define PLIC_ADDR_IRQ_PRIORITY(n) (0x00000000 + (4 * n))
#define PLIC_ADDR_CTX_ENABLE(n) (0x00002000 + (0x80 * n))
#define PLIC_ADDR_CTX_THRESHOLD(n) (0x00200000 + (0x00001000 * n))
#define PLIC_ADDR_CTX_CLAIM(n) (PLIC_ADDR_CTX_THRESHOLD(n) + 4)

#define PLIC_ADDR_MENABLE PLIC_ADDR_CTX_ENABLE(PLIC_MACHINE_CONTEXT)
#define PLIC_ADDR_SENABLE PLIC_ADDR_CTX_ENABLE(PLIC_SUPERVISOR_CONTEXT)
#define PLIC_ADDR_MTHRESHOLD PLIC_ADDR_CTX_THRESHOLD(PLIC_MACHINE_CONTEXT)
#define PLIC_ADDR_STHRESHOLD PLIC_ADDR_CTX_THRESHOLD(PLIC_SUPERVISOR_CONTEXT)
#define PLIC_ADDR_MCLAIM PLIC_ADDR_CTX_CLAIM(PLIC_MACHINE_CONTEXT)
#define PLIC_ADDR_SCLAIM PLIC_ADDR_CTX_CLAIM(PLIC_SUPERVISOR_CONTEXT)
#define PLIC_ADDR_MCOMPLETE PLIC_ADDR_MCLAIM
#define PLIC_ADDR_SCOMPLETE PLIC_ADDR_SCLAIM

typedef struct aclint_t {
  struct mmio_t base;
  unsigned long long mtime;
  unsigned long long mtimecmp;
  unsigned char msip;
  unsigned char ssip;
  unsigned cycle_count;
} aclint_t;

void aclint_init(aclint_t *);
char aclint_read(struct mmio_t *, unsigned addr);
void aclint_write(struct mmio_t *, unsigned addr, char value);
void aclint_cycle(aclint_t *);
void aclint_fini(aclint_t *);

#endif
