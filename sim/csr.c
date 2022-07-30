#include "sim.h"
#include "memory.h"
#include "csr.h"
#include <stdio.h>

#define CSR_INT_MEI_FIELD 11
#define CSR_INT_SEI_FIELD 9
#define CSR_INT_MTI_FIELD 7
#define CSR_INT_STI_FIELD 5
#define CSR_INT_MSI_FIELD 3
#define CSR_INT_SSI_FIELD 1

void csr_init(csr_t *csr) {
  csr->sim = NULL;
  csr->trap_handler = NULL;
  csr->hartid = 0;
  csr->mode = PRIVILEGE_MODE_M;
  csr->status_mpp = 0;
  csr->status_mie = 0;
  csr->status_mpie = 0;
  csr->status_spp = 0;
  csr->status_sie = 0;
  csr->status_spie = 0;
  // counters
  csr->cycle = 0;
  csr->time = 0;
  csr->timecmp = 0;
  csr->instret = 0;
  // trap & interrupts
  csr->mepc = 0;
  csr->mscratch = 0;
  csr->mtvec = 0;
  csr->mtval = 0;
  csr->mie = 0;
  csr->stvec = 0;
  csr->stval = 0;
  csr->sie = 0;
  return;
}

void csr_set_sim(csr_t *csr, sim_t *sim) {
  csr->sim = sim;
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
    {
      unsigned value = 0;
      value |= (csr->status_sie << 1);
      value |= (csr->status_spie << 5);
      value |= (csr->status_spp << 8);
      if (addr == CSR_ADDR_M_STATUS) {
        value |= (csr->status_mie << 3);
        value |= (csr->status_mpie << 7);
        value |= (csr->status_mpp << 11);
      }
      return value;
    }
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
  case CSR_ADDR_U_TIME:
    return (uint32_t)csr->time;
  case CSR_ADDR_U_TIMEH:
    return (uint32_t)(csr->time >> 32);
  case CSR_ADDR_M_IP:
  case CSR_ADDR_S_IP:
    {
      unsigned value = 0;
      unsigned swint = 0;
      unsigned extint = 0;
      unsigned timerint = (csr->time >= csr->timecmp) ? 1 : 0;
      value =
        (swint << CSR_INT_SSI_FIELD) |
        (extint << CSR_INT_SEI_FIELD) |
        (timerint << CSR_INT_STI_FIELD);
      if (addr == CSR_ADDR_M_IP) {
        value |=
          (swint << CSR_INT_MSI_FIELD) |
          (extint << CSR_INT_MEI_FIELD) |
          (timerint << CSR_INT_MTI_FIELD);
      }
      return value;
    }
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
    csr->status_mie = (value >> 3) & 0x00000001;
    csr->status_mpie = (value >> 7) & 0x00000001;
    csr->status_mpp = (value >> 11) & 0x00000003;
    // fall-through
  case CSR_ADDR_S_STATUS:
    csr->status_sie = (value >> 1) & 0x00000001;
    csr->status_spie = (value >> 5) & 0x00000001;
    csr->status_spp = (value >> 8) & 0x00000003;
    break;
  case CSR_ADDR_M_HARTID:
  case CSR_ADDR_U_TIME:
  case CSR_ADDR_U_TIMEH:
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

uint64_t csr_get_timecmp(csr_t *csr) {
  return csr->timecmp;
}

void csr_set_timecmp(csr_t *csr, uint64_t value) {
  csr->timecmp = value;
  return;
}

void csr_cycle(csr_t *csr, int n_instret) {
  csr->cycle++; // assume 100 MHz
  csr->instret += n_instret;
  if ((csr->cycle % 10) == 0) {
    csr->time++; // precision 0.1 us
  }
  int intr = 0;
  if (csr->mode == PRIVILEGE_MODE_M) {
    intr = csr_csrr(csr, CSR_ADDR_M_IE) & csr_csrr(csr, CSR_ADDR_M_IP);
    if (intr & (1 << CSR_INT_MTI_FIELD)) {
      csr_trap(csr, TRAP_CODE_M_TIMER_INTERRUPT);
    }
  } else if (csr->mode == PRIVILEGE_MODE_S) {
    intr = csr_csrr(csr, CSR_ADDR_S_IE) & csr_csrr(csr, CSR_ADDR_S_IP);
    if (intr & (1 << CSR_INT_STI_FIELD)) {
      csr_trap(csr, TRAP_CODE_S_TIMER_INTERRUPT);
    }
  }
  if (csr->status_mie && (csr->time >= csr->timecmp)) {
    csr_trap(csr, TRAP_CODE_M_TIMER_INTERRUPT);
  }
  return;
}

void csr_fini(csr_t *csr) {
  return;
}
