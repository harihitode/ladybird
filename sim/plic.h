#ifndef PLIC_H
#define PLIC_H

#include "memory.h"

typedef struct plic_t {
  struct mmio_t base;
  struct uart_t *uart;
  struct disk_t *disk;
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
char plic_read(struct mmio_t *, unsigned addr);
void plic_write(struct mmio_t *, unsigned addr, char value);
void plic_fini(plic_t *);

#define PLIC_ADDR_IRQ_PRIORITY(n) (0x00000000 + (4 * n))
#define PLIC_ADDR_CTX_ENABLE(n) (0x00002000 + (0x80 * n))
#define PLIC_ADDR_CTX_THRESHOLD(n) (0x00200000 + (0x00001000 * n))
#define PLIC_ADDR_CTX_CLAIM(n) (PLIC_ADDR_CTX_THRESHOLD(n) + 4)

#define PLIC_ADDR_DISK_PRIORITY PLIC_ADDR_IRQ_PRIORITY(1)
#define PLIC_ADDR_UART_PRIORITY PLIC_ADDR_IRQ_PRIORITY(10)
#define PLIC_ADDR_MENABLE PLIC_ADDR_CTX_ENABLE(0)
#define PLIC_ADDR_SENABLE PLIC_ADDR_CTX_ENABLE(1)
#define PLIC_ADDR_MTHRESHOLD PLIC_ADDR_CTX_THRESHOLD(0)
#define PLIC_ADDR_STHRESHOLD PLIC_ADDR_CTX_THRESHOLD(1)
#define PLIC_ADDR_MCOMPLETE PLIC_ADDR_MCLAIM
#define PLIC_ADDR_SCOMPLETE PLIC_ADDR_SCLAIM
#define PLIC_ADDR_MCLAIM PLIC_ADDR_CTX_CLAIM(0)
#define PLIC_ADDR_SCLAIM PLIC_ADDR_CTX_CLAIM(1)

#define PLIC_MAX_IRQ 10
#define PLIC_MACHINE_CONTEXT 0
#define PLIC_SUPERVISOR_CONTEXT 1

typedef struct aclint_t {
  struct mmio_t base;
  struct csr_t *csr;
} aclint_t;

void aclint_init(aclint_t *);
char aclint_read(struct mmio_t *, unsigned addr);
void aclint_write(struct mmio_t *, unsigned addr, char value);
void aclint_fini(aclint_t *);

#endif
