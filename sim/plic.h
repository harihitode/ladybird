#ifndef PLIC_H
#define PLIC_H

typedef struct plic_t {
  struct sim_t *sim;
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
char plic_read(plic_t *, unsigned addr);
void plic_write(plic_t *, unsigned addr, char value);
unsigned plic_get_interrupt(plic_t *, unsigned context);
void plic_fini(plic_t *);

#define PLIC_MAX_IRQ 10
#define PLIC_MACHINE_CONTEXT 0
#define PLIC_SUPERVISOR_CONTEXT 1

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
#define PLIC_ADDR_MCLAIM PLIC_ADDR_CTX_CLAIM(0)
#define PLIC_ADDR_SCLAIM PLIC_ADDR_CTX_CLAIM(1)
#define PLIC_ADDR_MCOMPLETE PLIC_ADDR_MCLAIM
#define PLIC_ADDR_SCOMPLETE PLIC_ADDR_SCLAIM

#endif
