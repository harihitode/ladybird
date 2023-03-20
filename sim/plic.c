#include "riscv.h"
#include "sim.h"
#include "plic.h"
#include "csr.h"
#include "mmio.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void plic_init(plic_t *plic) {
  plic->base.base = 0;
  plic->base.size = (1 << 24);
  plic->base.readb = plic_read;
  plic->base.writeb = plic_write;
  plic->base.get_irq = NULL;
  plic->base.ack_irq = NULL;
  plic->priorities = (unsigned *)malloc((PLIC_MAX_IRQ + 1) * sizeof(unsigned));
  plic->peripherals = (struct mmio_t **)calloc((PLIC_MAX_IRQ + 1), sizeof(struct mmio_t));
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
  case PLIC_ADDR_MENABLE:
    value = plic->m_interrupt_enable;
    break;
  case PLIC_ADDR_SENABLE:
    value = plic->s_interrupt_enable;
    break;
  case PLIC_ADDR_MTHRESHOLD:
    value = plic->m_interrupt_threshold;
    break;
  case PLIC_ADDR_STHRESHOLD:
    value = plic->s_interrupt_threshold;
    break;
  case PLIC_ADDR_SCLAIM:
    value = plic_get_interrupt(plic, PLIC_SUPERVISOR_CONTEXT);
    break;
  case PLIC_ADDR_MCLAIM:
    value = plic_get_interrupt(plic, PLIC_MACHINE_CONTEXT);
    break;
  default:
#if 0
    fprintf(stderr, "PLIC: unknown addr read: %08x\n", addr);
#endif
    break;
  }
  return (value >> (8 * woff));
}

void plic_write(struct mmio_t *unit, unsigned addr, char value) {
  addr -= unit->base;
  plic_t *plic = (plic_t *)unit;
  unsigned base = addr & 0xFFFFFFFC;
  unsigned woff = addr & 0x00000003;
  unsigned mask = 0x000000FF << (8 * woff);
  if (base >= PLIC_ADDR_IRQ_PRIORITY(0) && base <= PLIC_ADDR_IRQ_PRIORITY(PLIC_MAX_IRQ)) {
    unsigned irqno = ((base & 0x1fff) >> 2);
    plic->priorities[irqno] =
      (plic->priorities[irqno] & (~mask)) | ((unsigned char)value << (8 * woff));
  } else {
    switch (base) {
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
        unsigned irqno = plic_get_interrupt(plic, PLIC_SUPERVISOR_CONTEXT);
        if (plic->peripherals[irqno] && plic->peripherals[irqno]->ack_irq) {
          plic->peripherals[irqno]->ack_irq(plic->peripherals[irqno]);
        }
      }
      break;
    default:
#if 0
      fprintf(stderr, "PLIC: unknown addr write: %08x, %08x\n", addr, value);
#endif
      break;
    }
  }
  return;
}

void plic_set_peripheral(plic_t *plic, struct mmio_t *mmio, unsigned irqno) {
  if (irqno <= PLIC_MAX_IRQ) {
    plic->peripherals[irqno] = mmio;
  }
}

unsigned plic_get_interrupt(plic_t *plic, unsigned context) {
  unsigned enable = (context == PLIC_MACHINE_CONTEXT) ? plic->m_interrupt_enable : plic->s_interrupt_enable;
  unsigned threshold = (context == PLIC_MACHINE_CONTEXT) ? plic->m_interrupt_threshold : plic->s_interrupt_threshold;
  unsigned pending = 0;
  for (unsigned i = 1; i <= PLIC_MAX_IRQ; i++) {
    if (plic->peripherals[i] && plic->peripherals[i]->get_irq &&
        plic->peripherals[i]->get_irq(plic->peripherals[i])) {
      pending = pending | (1 << i);
    }
  }
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
  aclint->base.get_irq = NULL;
  aclint->base.ack_irq = NULL;
  aclint->csr = NULL;
}

char aclint_read(struct mmio_t *unit, unsigned addr) {
  aclint_t *aclint = (aclint_t *)unit;
  char value;
  unsigned long long byte_offset = addr & 0x7;
  unsigned long long value64 = 0;
  if (addr >= ACLINT_MSWI_BASE && addr < ACLINT_MSWI_BASE + (HART_NUM * 4)) {
    value64 = aclint->csr->software_interrupt_m;
  } else if (addr >= ACLINT_MTIMECMP_BASE && addr < ACLINT_MTIMECMP_BASE + (HART_NUM * 8)) {
    value64 = csr_get_timecmp(aclint->csr);
  } else if (addr >= ACLINT_MTIME_BASE && addr < ACLINT_MTIME_BASE + 8) {
    value64 = aclint->csr->time;
  } else {
    fprintf(stderr, "aclint read unimplemented region: %08x\n", addr);
  }
  value = (value64 >> (8 * byte_offset));
  return value;
}

void aclint_write(struct mmio_t *unit, unsigned addr, char value) {
  aclint_t *aclint = (aclint_t *)unit;
  if (addr >= ACLINT_MSWI_BASE && addr < ACLINT_MSWI_BASE + (HART_NUM * 4)) {
    if (addr == ACLINT_MSWI_BASE) {
      aclint->csr->software_interrupt_m = value;
    }
  } else if (addr >= ACLINT_MTIMECMP_BASE && addr < ACLINT_MTIMECMP_BASE + (HART_NUM * 8)) {
    // [TODO] currently hart0 only
    unsigned long long byte_offset = addr & 0x7;
    unsigned long long mask = (0x0FFL << (8 * byte_offset)) ^ 0xFFFFFFFFFFFFFFFF;
    unsigned long long timecmp = csr_get_timecmp(aclint->csr);
    timecmp = (timecmp & mask) | (((uint64_t)value << (8 * byte_offset)) & (0x0FFL << (8 * byte_offset)));
    csr_set_timecmp(aclint->csr, timecmp);
  } else if (addr >= ACLINT_MTIME_BASE && addr < 8) {
    // mtime read only
  } else {
    fprintf(stderr, "aclint write unimplemented region: %08x\n", addr);
  }
}

void aclint_fini(aclint_t *aclint) {
  return;
}
