#include "sim.h"
#include "memory.h"
#include "csr.h"
#include <stdio.h>

void csr_init(csr_t *csr) {
  csr->sim = NULL;
  csr->trap_handler = NULL;
  csr->hartid = 0;
  csr->mode = PRIVILEGE_MODE_M;
  //
  csr->mepc = 0;
  csr->sie = 0;
  csr->mscratch = 0;
  csr->mtvec = 0;
  csr->mtval = 0;
  csr->mie = 0;
  csr->stvec = 0;
  csr->stval = 0;
  csr->sie = 0;
  return;
}

unsigned csr_csrr(csr_t *csr, unsigned addr) {
  switch (addr) {
  case CSR_ADDR_M_EPC:
    return csr->mepc;
  case CSR_ADDR_M_HARTID:
    return csr->hartid;
  case CSR_ADDR_M_STATUS:
  case CSR_ADDR_S_STATUS:
    // TODO
    return 0;
  case CSR_ADDR_M_EDELEG:
  case CSR_ADDR_M_IDELEG:
  case CSR_ADDR_M_PMPCFG0:
  case CSR_ADDR_M_PMPADDR0:
    return 0; // not supported
  case CSR_ADDR_S_ATP:
    return
      (csr->sim->mem->vmflag << 30) |
      ((csr->sim->mem->vmbase >> 12) & 0x000fffff);
  case CSR_ADDR_S_IE:
    return csr->sie;
  case CSR_ADDR_S_TVEC:
    return csr->stvec;
  case CSR_ADDR_M_SCRATCH:
    return csr->mscratch;
  case CSR_ADDR_M_TVEC:
    return csr->mtvec;
  case CSR_ADDR_M_IE:
    return csr->mie;
  default:
    printf("unknown: CSR[R]: addr: %08x @%08x\n", addr, csr->sim->pc);
    return 0;
  }
}

void csr_csrw(csr_t *csr, unsigned addr, unsigned value) {
  switch (addr) {
  case CSR_ADDR_M_EPC:
    csr->mepc = value;
    break;
  case CSR_ADDR_M_STATUS:
  case CSR_ADDR_S_STATUS:
    // TODO
    break;
  case CSR_ADDR_M_HARTID:
    break; // read only
  case CSR_ADDR_M_EDELEG:
  case CSR_ADDR_M_IDELEG:
  case CSR_ADDR_M_PMPCFG0:
  case CSR_ADDR_M_PMPADDR0:
    break; // not supported
  case CSR_ADDR_S_ATP:
    if (value & 0x80000000) {
      memory_atp_on(csr->sim->mem, value & 0x000fffff);
    } else {
      memory_atp_off(csr->sim->mem);
    }
    break;
  case CSR_ADDR_S_IE:
    csr->sie = value;
    break;
  case CSR_ADDR_S_TVEC:
    csr->stvec = value;
    break;
  case CSR_ADDR_M_SCRATCH:
    csr->mscratch = value;
    break;
  case CSR_ADDR_M_TVEC:
    csr->mtvec = value;
    break;
  case CSR_ADDR_M_IE:
    csr->mie = value;
    break;
  default:
    printf("unknown: CSR[W]: addr: %08x value: %08x @%08x\n", addr, value, csr->sim->pc);
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

void csr_trap(csr_t *csr, unsigned trap_code) {
  csr->mcause = trap_code;
  if (csr->trap_handler) csr->trap_handler(csr->sim);
}

void csr_set_tval(csr_t *csr, unsigned trap_value) {
  if (csr->mode == PRIVILEGE_MODE_M) {
    csr->mtval = trap_value;
  } else {
    csr->stval = trap_value;
  }
  return;
}

unsigned csr_get_tval(csr_t *csr) {
  if (csr->mode == PRIVILEGE_MODE_M) {
    return csr->mtval;
  } else {
    return csr->stval;
  }
}

void csr_fini(csr_t *csr) {
  return;
}
