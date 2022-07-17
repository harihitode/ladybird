#include "csr.h"
#include <stdio.h>

void csr_init(csr_t *csr) {
  csr->mepc = 0;
  return;
}

unsigned csr_csrr(csr_t *csr, unsigned addr) {
  printf("CSR[R]: addr: %08x\n", addr);
  switch (addr) {
  case CSR_ADDR_M_EPC:
    return csr->mepc;
  default:
    return 0;
  }
}

void csr_csrw(csr_t *csr, unsigned addr, unsigned value) {
  printf("CSR[W]: addr: %08x value: %08x\n", addr, value);
  switch (addr) {
  case CSR_ADDR_M_EPC:
    csr->mepc = value;
    break;
  default:
    break;
  }
  return;
}

unsigned csr_csrrw(csr_t *csr, unsigned addr, unsigned value) {
  unsigned csr_read_value = csr_csrr(csr, addr);
  csr_csrw(csr, addr, value);
  return csr_read_value;
}

unsigned csr_csrrs(csr_t *csr, unsigned addr, unsigned value) {
  unsigned csr_read_value = csr_csrr(csr, addr);
  csr_csrw(csr, addr, value | csr_read_value);
  return csr_read_value;
}

unsigned csr_csrrc(csr_t *csr, unsigned addr, unsigned value) {
  unsigned csr_read_value = csr_csrr(csr, addr);
  csr_csrw(csr, addr, (~value) & csr_read_value);
  return csr_read_value;
}

void csr_fini(csr_t *csr) {
  return;
}
