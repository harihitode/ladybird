#include "riscv.h"
#include "plic.h"
#include "csr.h"
#include "mmio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void plic_init (plic_t *plic) {
  plic->base.base = 0;
  plic->base.size = (1 << 24);
  plic->base.readb = plic_read;
  plic->base.writeb = plic_write;
  plic->priorities = (unsigned *)malloc((PLIC_MAX_IRQ + 1) * sizeof(unsigned));
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

char plic_read(struct mmio_t *unit, unsigned addr) {
  addr -= unit->base;
  plic_t *plic = (plic_t *)unit;
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
    fprintf(stderr, "PLIC: unknown addr read: %08x\n", addr);
    break;
  }
  return value;
}

void plic_write(struct mmio_t *unit, unsigned addr, char value) {
  addr -= unit->base;
  plic_t *plic = (plic_t *)unit;
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
    fprintf(stderr, "PLIC: unknown addr write: %08x, %08x\n", addr, value);
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

void aclint_init(aclint_t *aclint) {
  aclint->base.base = 0;
  aclint->base.size = (1 << 16);
  aclint->base.readb = aclint_read;
  aclint->base.writeb = aclint_write;
  aclint->csr = NULL;
}

char aclint_read(struct mmio_t *unit, unsigned addr) {
  addr -= unit->base;
  aclint_t *aclint = (aclint_t *)unit;
  char value;
  uint64_t byte_offset = addr % 8;
  uint64_t value64 = 0;
  if (addr < 0x0000BFF8) {
    value64 = csr_get_timecmp(aclint->csr);
  } else if (addr <= 0x0000BFFF) {
    value64 =
      ((uint64_t)csr_csrr(aclint->csr, CSR_ADDR_U_TIMEH, NULL) << 32) |
      ((uint64_t)csr_csrr(aclint->csr, CSR_ADDR_U_TIME, NULL));
  } else {
    fprintf(stderr, "aclint read unimplemented region: %08x\n", addr);
  }
  value = (value64 >> (8 * byte_offset));
  return value;
}

void aclint_write(struct mmio_t *unit, unsigned addr, char value) {
  addr -= unit->base;
  aclint_t *aclint = (aclint_t *)unit;
  if (addr < 0x0000BFF8) {
    // hart 0 mtimecmp
    uint64_t byte_offset = addr % 8;
    uint64_t mask = (0x0FFL << (8 * byte_offset)) ^ 0xFFFFFFFFFFFFFFFF;
    uint64_t timecmp = csr_get_timecmp(aclint->csr);
    timecmp = (timecmp & mask) | (((uint64_t)value << (8 * byte_offset)) & (0xFFL << (8 * byte_offset)));
    csr_set_timecmp(aclint->csr, timecmp);
  } else if (addr <= 0x0000BFFF) {
    // mtime read only
  } else {
    fprintf(stderr, "aclint write unimplemented region: %08x\n", addr);
  }
}

void aclint_fini(aclint_t *aclint) {
  return;
}
