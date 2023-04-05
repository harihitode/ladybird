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
  plic->num_hart = 0;
  plic->priorities = (unsigned *)malloc((PLIC_MAX_IRQ + 1) * sizeof(unsigned));
  plic->peripherals = (struct mmio_t **)calloc((PLIC_MAX_IRQ + 1), sizeof(struct mmio_t));
  plic->interrupt_enable = NULL;
  plic->interrupt_threshold = NULL;
  plic->interrupt_complete = NULL;
  return;
}

void plic_add_hart(plic_t *plic) {
  if (plic->num_hart == 0) {
    plic->interrupt_enable = (unsigned *)malloc(2 * sizeof(unsigned));
    plic->interrupt_threshold = (unsigned *)malloc(2 * sizeof(unsigned));
    plic->interrupt_complete = (unsigned *)malloc(2 * sizeof(unsigned));
  } else {
    plic->interrupt_enable = (unsigned *)realloc(plic->interrupt_enable, 2 * (plic->num_hart + 1) * sizeof(unsigned));
    plic->interrupt_threshold = (unsigned *)realloc(plic->interrupt_threshold, 2 * (plic->num_hart + 1) * sizeof(unsigned));
    plic->interrupt_complete = (unsigned *)realloc(plic->interrupt_complete, 2 * (plic->num_hart + 1) * sizeof(unsigned));
  }
  plic->num_hart++;
}

char plic_read(struct mmio_t *unit, unsigned addr) {
  addr -= unit->base;
  plic_t *plic = (plic_t *)unit;
  unsigned woff = addr & 0x00000003;
  unsigned value = 0;
  if (addr >= PLIC_ADDR_CTX_ENABLE_BASE &&
      addr < (PLIC_ADDR_CTX_ENABLE_BASE + (2 * plic->num_hart * 0x080))) {
    unsigned context_id = (addr - PLIC_ADDR_CTX_ENABLE_BASE) / 0x080;
    value = plic->interrupt_enable[context_id];
  } else if (addr >= PLIC_ADDR_CTX_THRESHOLD_BASE &&
             addr < (PLIC_ADDR_CTX_THRESHOLD_BASE + (2 * plic->num_hart * 0x1000))) {
    unsigned context_id = (addr - PLIC_ADDR_CTX_THRESHOLD_BASE) / 0x1000;
    if ((addr & 0x7) < 4) { // threashold
      value = plic->interrupt_threshold[context_id];
    } else { // claim
      value = plic_get_interrupt(plic, context_id);
    }
  } else {
    fprintf(stderr, "PLIC: unknown addr read: %08x\n", addr);
  }
  return (value >> (8 * woff));
}

void plic_write(struct mmio_t *unit, unsigned addr, char value) {
  addr -= unit->base;
  plic_t *plic = (plic_t *)unit;
  unsigned woff = addr & 0x00000003;
  unsigned mask = 0x000000FF << (8 * woff);
  if (addr >= PLIC_ADDR_IRQ_PRIORITY_BASE &&
      addr < (PLIC_ADDR_IRQ_PRIORITY_BASE + (PLIC_MAX_IRQ + 1) * 0x4)) {
    unsigned irqno = ((addr & 0x1fff) >> 2);
    plic->priorities[irqno] =
      (plic->priorities[irqno] & (~mask)) | ((unsigned char)value << (8 * woff));
  } else if (addr >= PLIC_ADDR_CTX_ENABLE_BASE &&
             addr < (PLIC_ADDR_CTX_ENABLE_BASE + (2 * plic->num_hart * 0x080))) {
    unsigned context_id = (addr - PLIC_ADDR_CTX_ENABLE_BASE) / 0x080;
    plic->interrupt_enable[context_id] =
      (plic->interrupt_enable[context_id] & (~mask)) | ((unsigned char)value << (8 * woff));
  } else if (addr >= PLIC_ADDR_CTX_THRESHOLD_BASE &&
             addr < (PLIC_ADDR_CTX_THRESHOLD_BASE + (2 * plic->num_hart * 0x1000))) {
    unsigned context_id = (addr - PLIC_ADDR_CTX_THRESHOLD_BASE) / 0x1000;
    if ((addr & 0x7) < 4) { // threashold
      plic->interrupt_threshold[context_id] =
        (plic->interrupt_threshold[context_id] & (~mask)) | ((unsigned char)value << (8 * woff));
    } else { // complete
      plic->interrupt_complete[context_id] =
        (plic->interrupt_complete[context_id] & (~mask)) | ((unsigned char)value << (8 * woff));
      if (woff == 0 && plic->interrupt_complete[context_id] <= PLIC_MAX_IRQ) {
        unsigned irqno = plic->interrupt_complete[context_id];
        if (plic->peripherals[irqno] && plic->peripherals[irqno]->ack_irq) {
          plic->peripherals[irqno]->ack_irq(plic->peripherals[irqno]);
        }
      }
    }
  } else {
    fprintf(stderr, "PLIC: unknown addr write: %08x, %08x\n", addr, value);
  }
  return;
}

void plic_set_peripheral(plic_t *plic, struct mmio_t *mmio, unsigned irqno) {
  if (irqno <= PLIC_MAX_IRQ) {
    plic->peripherals[irqno] = mmio;
  }
}

unsigned plic_get_interrupt(plic_t *plic, unsigned context) {
  unsigned enable = plic->interrupt_enable[context];
  unsigned threshold = plic->interrupt_threshold[context];
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
  free(plic->interrupt_enable);
  free(plic->interrupt_threshold);
  free(plic->interrupt_complete);
  return;
}

void aclint_init(aclint_t *aclint) {
  aclint->base.base = 0;
  aclint->base.size = (1 << 16);
  aclint->base.readb = aclint_read;
  aclint->base.writeb = aclint_write;
  aclint->base.get_irq = NULL;
  aclint->base.ack_irq = NULL;
  aclint->num_hart = 0;
  aclint->mtime = 0;
  aclint->mtimecmp = NULL;
  aclint->msip = NULL;
  aclint->ssip = NULL;
  aclint->cycle_count = 0;
}

void aclint_add_hart(aclint_t *aclint) {
  if (aclint->num_hart == 0) {
    aclint->mtimecmp = (unsigned long long *)malloc(sizeof(unsigned long long));
    aclint->msip = (unsigned char *)malloc(sizeof(unsigned char));
    aclint->ssip = (unsigned char *)malloc(sizeof(unsigned char));
  } else {
    aclint->mtimecmp = (unsigned long long *)realloc(aclint->mtimecmp, (aclint->num_hart + 1) * sizeof(unsigned long long));
    aclint->msip = (unsigned char *)realloc(aclint->msip, (aclint->num_hart + 1) * sizeof(unsigned char));
    aclint->ssip = (unsigned char *)realloc(aclint->ssip, (aclint->num_hart + 1) * sizeof(unsigned char));
  }
  aclint->num_hart++;
}

char aclint_read(struct mmio_t *unit, unsigned addr) {
  aclint_t *aclint = (aclint_t *)unit;
  char value;
  unsigned long long byte_offset = 0;
  unsigned long long value64 = 0;
  if (addr >= ACLINT_MSIP_BASE &&
      addr < ACLINT_MSIP_BASE + (aclint->num_hart * 4)) {
    unsigned hart_id = (addr - ACLINT_MSIP_BASE) / 4;
    byte_offset = addr & 0x3;
    value64 = aclint->msip[hart_id];
  } else if (addr >= ACLINT_SETSSIP_BASE && addr < ACLINT_SETSSIP_BASE + (aclint->num_hart * 4)) {
    unsigned hart_id = (addr - ACLINT_SETSSIP_BASE) / 4;
    byte_offset = addr & 0x3;
    value64 = aclint->ssip[hart_id];
  } else if (addr >= ACLINT_MTIMECMP_BASE && addr < ACLINT_MTIMECMP_BASE + (aclint->num_hart * 8)) {
    unsigned hart_id = (addr - ACLINT_MTIMECMP_BASE) / 8;
    byte_offset = addr & 0x7;
    value64 = aclint->mtimecmp[hart_id];
  } else if (addr >= ACLINT_MTIME_BASE && addr < ACLINT_MTIME_BASE + 8) {
    byte_offset = addr & 0x7;
    value64 = aclint->mtime;
  } else {
    fprintf(stderr, "aclint read unimplemented region: %08x\n", addr);
  }
  value = (value64 >> (8 * byte_offset));
  return value;
}

void aclint_write(struct mmio_t *unit, unsigned addr, char value) {
  aclint_t *aclint = (aclint_t *)unit;
  if (addr >= ACLINT_MSIP_BASE &&
      addr < ACLINT_MSIP_BASE + (aclint->num_hart * 4)) {
    unsigned hart_id = (addr - ACLINT_MSIP_BASE) / 4;
    if ((addr & 3) == 0) {
      aclint->msip[hart_id] = value;
    }
  } else if (addr >= ACLINT_SETSSIP_BASE &&
             addr < ACLINT_SETSSIP_BASE + (aclint->num_hart * 4)) {
    unsigned hart_id = (addr - ACLINT_SETSSIP_BASE) / 4;
    if ((addr & 3) == 0 && value == 1) {
      aclint->ssip[hart_id] = 1; // edge triggered
    }
  } else if (addr >= ACLINT_MTIMECMP_BASE &&
             addr < ACLINT_MTIMECMP_BASE + (aclint->num_hart * 8)) {
    unsigned hart_id = (addr - ACLINT_MTIMECMP_BASE) / 8;
    unsigned long long byte_offset = addr & 0x7;
    unsigned long long mask = (0x0FFL << (8 * byte_offset)) ^ 0xFFFFFFFFFFFFFFFF;
    aclint->mtimecmp[hart_id] =
      ((aclint->mtimecmp[hart_id] & mask) |
       (((uint64_t)value << (8 * byte_offset)) & (0x0FFL << (8 * byte_offset))));
  } else if (addr >= ACLINT_MTIME_BASE && addr < 8) {
    // mtime read only
  } else {
    fprintf(stderr, "aclint write unimplemented region: %08x\n", addr);
  }
}

void aclint_cycle(aclint_t *aclint) {
  if (aclint->cycle_count++ % 10 == 0) {
    aclint->mtime++;
  }
}

unsigned long long aclint_get_mtimecmp(aclint_t *aclint, int hart_id) {
  return aclint->mtimecmp[hart_id];
}

unsigned aclint_get_msip(aclint_t *aclint, int hart_id) {
  return aclint->msip[hart_id];
}

unsigned aclint_get_ssip(aclint_t *aclint, int hart_id) {
  return aclint->ssip[hart_id];
}

void aclint_set_msip(aclint_t *aclint, int hart_id, unsigned char val) {
  aclint->msip[hart_id] = val;
}

void aclint_set_ssip(aclint_t *aclint, int hart_id, unsigned char val) {
  aclint->ssip[hart_id] = val;
}

void aclint_fini(aclint_t *aclint) {
  free(aclint->mtimecmp);
  free(aclint->msip);
  free(aclint->ssip);
  return;
}
