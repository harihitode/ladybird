#include "sim.h"
#include "memory.h"
#include "csr.h"
#include "plic.h"
#include <stdio.h>
#include <stdlib.h>

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
  csr->exception = 0;
  csr->exception_code = 0;
  csr->interrupts_enable = 0;
  csr->mideleg = 0;
  csr->medeleg = 0;
  csr->mscratch = 0;
  csr->mepc = 0;
  csr->mcause = 0;
  csr->mtval = 0;
  csr->mtvec = 0;
  csr->sscratch = 0;
  csr->sepc = 0;
  csr->scause = 0;
  csr->stval = 0;
  csr->stvec = 0;
  // SW interrupts
  csr->software_interrupt_m = 0;
  csr->software_interrupt_s = 0;
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
    return csr->medeleg;
  case CSR_ADDR_M_IDELEG:
    return csr->mideleg;
  case CSR_ADDR_M_PMPCFG0:
  case CSR_ADDR_M_PMPADDR0:
    return 0; // not supported
  case CSR_ADDR_S_ATP:
    return
      (csr->sim->mem->vmflag << 31) |
      ((csr->sim->mem->vmrppn >> 12) & 0x000fffff);
  case CSR_ADDR_S_IE:
    return csr->interrupts_enable & 0x00000222;
  case CSR_ADDR_S_TVEC:
    return csr->stvec;
  case CSR_ADDR_M_SCRATCH:
    return csr->mscratch;
  case CSR_ADDR_M_TVEC:
    return csr->mtvec;
  case CSR_ADDR_M_IE:
    return csr->interrupts_enable;
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
      unsigned timerint = 0;
      extint = (plic_get_interrupt(csr->sim->mem->plic, PLIC_SUPERVISOR_CONTEXT) == 0) ? 0 : 1;
      timerint = 0;
      swint = csr->software_interrupt_s;
      value =
        (swint << CSR_INT_SSI_FIELD) |
        (extint << CSR_INT_SEI_FIELD) |
        (timerint << CSR_INT_STI_FIELD);
      if (addr == CSR_ADDR_M_IP) {
        extint = (plic_get_interrupt(csr->sim->mem->plic, PLIC_MACHINE_CONTEXT) == 0) ? 0 : 1;
        timerint = (csr->time >= csr->timecmp) ? 1 : 0;
        swint = csr->software_interrupt_m;
        value |=
          (swint << CSR_INT_MSI_FIELD) |
          (extint << CSR_INT_MEI_FIELD) |
          (timerint << CSR_INT_MTI_FIELD);
      }
      return value;
    }
  case CSR_ADDR_S_EPC:
    return csr->sepc;
  case CSR_ADDR_S_CAUSE:
    return csr->scause;
  case CSR_ADDR_S_TVAL:
    return csr->stval;
  case CSR_ADDR_S_SCRATCH:
    return csr->sscratch;
  default:
    fprintf(stderr, "unknown: CSR[R]: addr: %08x @%08x\n", addr, csr->sim->pc);
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
    // exception delegation
    csr->medeleg = value;
    break;
  case CSR_ADDR_M_IDELEG:
    // interrupt delegation
    // this is a litte bit confusing.
    // the machine mode interrupts could not be delegated to supervisor
    // I don't know either it is a qemu ristriction or a spec. of risc-v
    csr->mideleg = value & 0x0000222;
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
    csr->interrupts_enable = (csr->interrupts_enable & 0x00000888) | value;
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
    csr->interrupts_enable = value;
    break;
  case CSR_ADDR_S_EPC:
    csr->sepc = value;
    break;
  case CSR_ADDR_S_CAUSE:
    csr->scause = value;
    break;
  case CSR_ADDR_M_IP:
    csr->software_interrupt_m = (value >> 3) & 0x00000001;
    // fall-through
  case CSR_ADDR_S_IP:
    csr->software_interrupt_s = (value >> 1) & 0x00000001;
    break;
  case CSR_ADDR_S_TVAL:
    csr->stval = value;
    break;
  case CSR_ADDR_S_SCRATCH:
    csr->sscratch = value;
    break;
  default:
    fprintf(stderr, "unknown: CSR[W]: addr: %08x value: %08x @%08x\n", addr, value, csr->sim->pc);
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
  unsigned is_interrupt = ((trap_code >> 31) & 0x00000001);
  unsigned interrupt_code = (trap_code & 0x7fffffff);
  unsigned exception_code = (trap_code & 0x7fffffff);
  unsigned delegation_to_s = 0;
  unsigned is_delegate = 0;
  // delegation check
  if (is_interrupt) {
    delegation_to_s = (csr->mideleg >> interrupt_code) & 0x00000001;
  } else {
    delegation_to_s = (csr->medeleg >> exception_code) & 0x00000001;
  }
  if (is_interrupt) {
    // delegation check
    if (
        // (a) if either the current privilege mode is M and the MIE bit in the mstatus register is set, or the current privilege mode has less privilege than M-mode
        ((csr->mode == PRIVILEGE_MODE_M && csr->status_mie) ||
         (csr->mode < PRIVILEGE_MODE_M)) &&
        // (b) interrupt bit is set in both mip and mie
        ((((csr_csrr(csr, CSR_ADDR_M_IP) & csr_csrr(csr, CSR_ADDR_M_IE)) >> interrupt_code)) & 0x00000001) &&
        // (c) interrupt is not set in mideleg
        !((csr->mideleg >> interrupt_code) & 0x00000001)
        ) {
      is_delegate = 0;
    } else {
      is_delegate = 1;
    }
  } else {
    if (delegation_to_s) {
      is_delegate = 1;
    } else {
      is_delegate = 0;
    }
  }

  // default: to Machine Mode
  unsigned to_mode = (is_delegate) ? PRIVILEGE_MODE_S : PRIVILEGE_MODE_M;
  if (to_mode == PRIVILEGE_MODE_M) {
    csr->mcause = trap_code;
    // pc
    csr->mepc = csr->sim->pc;
    csr->sim->pc = csr->mtvec;
    // enable
    csr->status_mpie = csr->status_mie;
    csr->status_mie = 0;
    // mode
    csr->status_mpp = csr->mode;
    csr->mode = to_mode;
#if 0
    fprintf(stderr, "[to M] trap from %d to %d: code: %08x, %08x\n", csr->status_mpp, to_mode, trap_code, csr->mideleg);
#endif
  } else {
    csr->scause = trap_code;
    // pc
    csr->sepc = csr->sim->pc;
    csr->sim->pc = csr->stvec;
    // enable
    csr->status_spie = csr->status_sie;
    csr->status_sie = 0;
    // mode
    csr->status_spp = csr->mode;
    csr->mode = to_mode;
#if 0
    fprintf(stderr, "[to S] trap from %d to %d: code: %08x (PC is set %08x)\n", csr->status_spp, to_mode, trap_code, csr->sim->pc);
#endif
  }
  if (csr->trap_handler) csr->trap_handler(csr->sim);
  return;
}

void csr_trapret(csr_t *csr) {
  unsigned from_mode = csr->mode;
  if (from_mode == PRIVILEGE_MODE_M) {
    // pc
    csr->sim->pc = csr->mepc;
    csr->mepc = 0;
    // enable
    csr->status_mie = csr->status_mpie;
    csr->status_mpie = 1;
    // mode
    csr->mode = csr->status_mpp;
    csr->status_mpp = 0;
  } else {
    // pc
    csr->sim->pc = csr->sepc;
    csr->sepc = 0;
    // enable
    csr->status_sie = csr->status_spie;
    csr->status_spie = 1;
    // mode
    csr->mode = csr->status_spp;
    csr->status_spp = 0;
  }
  return;
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

void csr_exception(csr_t *csr, unsigned code) {
  csr->exception = 1;
  csr->exception_code = code;
  return;
}

void csr_cycle(csr_t *csr, int n_instret) {
  csr->cycle++; // assume 100 MHz
  csr->instret += n_instret;
  if ((csr->cycle % 10) == 0) {
    csr->time++; // precision 0.1 us
  }
  unsigned interrupts_bits [6] =
    {
     CSR_INT_MEI_FIELD,
     CSR_INT_MSI_FIELD,
     CSR_INT_MTI_FIELD,
     CSR_INT_SEI_FIELD,
     CSR_INT_SSI_FIELD,
     CSR_INT_STI_FIELD
    };
  unsigned interrupts_code [6] =
    {
     TRAP_CODE_M_EXTERNAL_INTERRUPT,
     TRAP_CODE_M_SOFTWARE_INTERRUPT,
     TRAP_CODE_M_TIMER_INTERRUPT,
     TRAP_CODE_S_EXTERNAL_INTERRUPT,
     TRAP_CODE_S_SOFTWARE_INTERRUPT,
     TRAP_CODE_S_TIMER_INTERRUPT
    };
  unsigned interrupts_pending = csr_csrr(csr, CSR_ADDR_M_IP);
  unsigned global_interrupts_enable_m = 0;
  unsigned global_interrupts_enable_s = 0;
  if (csr->mode == PRIVILEGE_MODE_M) {
    global_interrupts_enable_m = csr->status_mie;
  } else {
    global_interrupts_enable_m = 1;
  }
  if (csr->mode > PRIVILEGE_MODE_S) {
    global_interrupts_enable_s = 0;
  } else if (csr->mode == PRIVILEGE_MODE_S) {
    global_interrupts_enable_s = csr->status_sie;
  } else {
    global_interrupts_enable_s = 1;
  }

  unsigned interrupts_enable =
    csr_csrr(csr, CSR_ADDR_M_IE) &
    ((global_interrupts_enable_m << CSR_INT_MEI_FIELD) |
     (global_interrupts_enable_m << CSR_INT_MSI_FIELD) |
     (global_interrupts_enable_m << CSR_INT_MTI_FIELD) |
     (global_interrupts_enable_s << CSR_INT_SEI_FIELD) |
     (global_interrupts_enable_s << CSR_INT_SSI_FIELD) |
     (global_interrupts_enable_s << CSR_INT_STI_FIELD));
  int intr = ((interrupts_pending & interrupts_enable) == 0) ? 0 : 1;
  unsigned trap_code = 0;
  for (int i = 0; i < 6; i++) {
    if (((interrupts_pending & interrupts_enable) >> interrupts_bits[i]) & 0x00000001) {
      trap_code = interrupts_code[i];
      // Simultaneous interrupts destined for M-mode are handled in the following
      // decreasing priority order: MEI, MSI, MTI, SEI, SSI, STI
      // degelation check
      break;
    }
  }
  if (csr->exception) {
    csr->exception = 0;
    csr_trap(csr, csr->exception_code);
  } else if (intr) {
#if 0
    fprintf(stderr, "mode: %d, code %08x, gmie: %d, gsie: %d, mie: %08x sie: %08x, ip:%08x\n", csr->mode, trap_code, global_interrupts_enable_m, global_interrupts_enable_s, csr_csrr(csr, CSR_ADDR_M_IE), csr_csrr(csr, CSR_ADDR_S_IE), interrupts_pending);
    fprintf(stderr, "%016lx, %016lx\n", csr->time, csr->timecmp);
#endif
    csr_trap(csr, trap_code);
  }
  return;
}

void csr_fini(csr_t *csr) {
  return;
}
