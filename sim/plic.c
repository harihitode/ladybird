#include "plic.h"
#include "sim.h"
#include "mmio.h"
#include <stdio.h>
#include <stdlib.h>

void plic_init (plic_t *plic) {
  plic->priorities = (unsigned *)malloc(PLIC_MAX_IRQ * sizeof(unsigned));
  plic->uart = NULL;
  plic->disk = NULL;
  plic->s_interrupt_enable = 0;
  plic->s_interrupt_threshold = 0;
  plic->s_interrupt_complete = 0;
  plic->m_interrupt_enable = 0;
  plic->m_interrupt_threshold = 0;
  plic->m_interrupt_complete = 0;
  return;
}

char plic_read(plic_t *plic, unsigned addr) {
  unsigned base = addr & 0xFFFFFFFC;
  unsigned woff = addr & 0x00000003;
  unsigned value = 0;
  switch (base) {
  case PLIC_ADDR_SCLAIM:
    {
      unsigned irq = plic_get_interrupt(plic, PLIC_SUPERVISOR_CONTEXT);
      value = irq >> (8 * woff);
    }
    break;
  default:
    printf("PLIC: unknown addr read: %08x\n", addr);
    break;
  }
  return value;
}

void plic_write(plic_t *plic, unsigned addr, char value) {
  unsigned base = addr & 0xFFFFFFFC;
  unsigned woff = addr & 0x00000003;
  unsigned mask = 0x000000FF << (8 * woff);
  switch (base) {
  case PLIC_ADDR_UART_PRIORITY:
    plic->priorities[1] =
      (plic->priorities[1] & (~mask)) | ((unsigned char)value << (8 * woff));
    break;
  case PLIC_ADDR_DISK_PRIORITY:
    plic->priorities[10] =
      (plic->priorities[10] & (~mask)) | ((unsigned char)value << (8 * woff));
    break;
  case PLIC_ADDR_SENABLE:
    plic->s_interrupt_enable =
      (plic->s_interrupt_enable & (~mask)) | ((unsigned char)value << (8 * woff));
    break;
  case PLIC_ADDR_STHRESHOLD:
    plic->s_interrupt_threshold =
      (plic->s_interrupt_threshold & (~mask)) | ((unsigned char)value << (8 * woff));
    break;
  case PLIC_ADDR_MENABLE:
    plic->m_interrupt_enable =
      (plic->m_interrupt_enable & (~mask)) | ((unsigned char)value << (8 * woff));
    break;
  case PLIC_ADDR_MTHRESHOLD:
    plic->m_interrupt_threshold =
      (plic->m_interrupt_threshold & (~mask)) | ((unsigned char)value << (8 * woff));
    break;
  case PLIC_ADDR_SCOMPLETE:
    plic->s_interrupt_complete =
      (plic->s_interrupt_complete & (~mask)) | ((unsigned char)value << (8 * woff));
    if (woff == 0 && plic->s_interrupt_complete) {
      unsigned irq = plic_get_interrupt(plic, PLIC_SUPERVISOR_CONTEXT);
      if (irq == 1) {
        disk_irq_ack(plic->disk);
      } else if (irq == 10) {
        uart_irq_ack(plic->uart);
      }
    }
    break;
  default:
    printf("PLIC: unknown addr write: %08x, %08x\n", addr, value);
    break;
  }
  return;
}

// TODO: more cool way for arbitrary irq
unsigned plic_get_interrupt(plic_t *plic, unsigned context) {
  unsigned enable = (context == 0) ? plic->m_interrupt_enable : plic->s_interrupt_enable;
  unsigned threshold = (context == 0) ? plic->m_interrupt_threshold : plic->s_interrupt_threshold;
  unsigned irq1 = disk_irq(plic->disk);
  unsigned irq10 = uart_irq(plic->uart);
  unsigned pending = (irq10 << 10) | (irq1 << 1);
  unsigned irq_id = 0;
  for (unsigned i = 1; i <= PLIC_MAX_IRQ; i++) {
    if ((((enable & pending) >> i) & 0x00000001) && (plic->priorities[i] > threshold)) {
      irq_id = i;
      break;
    }
  }
  return irq_id;
}

void plic_fini(plic_t *plic) {
  free(plic->priorities);
  return;
}
