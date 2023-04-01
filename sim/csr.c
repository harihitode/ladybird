#include "riscv.h"
#include "sim.h"
#include "memory.h"
#include "csr.h"
#include "plic.h"
#include "core.h"
#include "lsu.h"
#include "trigger.h"
#include <stdio.h>
#include <stdlib.h>

#define CSR_NUM_HPM 29

void csr_init(csr_t *csr) {
  csr->lsu = NULL;
  csr->plic = NULL;
  csr->aclint = NULL;
  csr->trig = NULL;
  csr->hart_id = 0;
  csr->mode = PRIVILEGE_MODE_M;
  csr->status_mpp = 0;
  csr->status_mie = 0;
  csr->status_mpie = 0;
  csr->status_spp = 0;
  csr->status_sie = 0;
  csr->status_spie = 0;
  csr->status_sum = 0;
  // counters
  csr->cycle = 0;
  csr->instret = 0;
  // trap & interrupts
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
  csr->mcounteren = 0;
  csr->scounteren = 0;
  // Timer interrupts
  csr->stip = 0;
  // debug
  csr->dcsr_ebreakm = 0;
  csr->dcsr_ebreaks = 0;
  csr->dcsr_ebreaku = 0;
  csr->dcsr_step = 0;
  csr->dcsr_mprven = 0;
  csr->dcsr_prv = PRIVILEGE_MODE_M;
  csr->dpc = 0;
  csr->tselect = 0;
  for (int i = 0; i < CSR_NUM_HPM; i++) {
    csr->hpmcounter[i] = 0;
    csr->hpmevent[i] = 0;
  }
  csr->regstat_en = 0;
  return;
}

static unsigned csr_get_s_interrupts_pending(csr_t *csr) {
  unsigned value = 0;
  unsigned swint = 0;
  unsigned extint = 0;
  unsigned timerint = 0;
  extint = (plic_get_interrupt(csr->plic, PLIC_SUPERVISOR_CONTEXT) == 0) ? 0 : 1;
  timerint = csr->stip;
  swint = csr->aclint->ssip;
  value =
    (swint << CSR_INT_SSI_FIELD) |
    (extint << CSR_INT_SEI_FIELD) |
    (timerint << CSR_INT_STI_FIELD);
  return value;
}

static unsigned csr_get_m_interrupts_pending(csr_t *csr) {
  unsigned value = csr_get_s_interrupts_pending(csr);
  unsigned swint = 0;
  unsigned extint = 0;
  unsigned timerint = 0;
  extint = (plic_get_interrupt(csr->plic, PLIC_MACHINE_CONTEXT) == 0) ? 0 : 1;
  timerint = (csr->aclint->mtime >= csr->aclint->mtimecmp) ? 1 : 0;
  swint = csr->aclint->msip;
  value |=
    (swint << CSR_INT_MSI_FIELD) |
    (extint << CSR_INT_MEI_FIELD) |
    (timerint << CSR_INT_MTI_FIELD);
  return value;
}

unsigned csr_csrr(csr_t *csr, unsigned addr, struct core_step_result *result) {
  switch (addr) {
  case CSR_ADDR_M_EPC:
    return csr->mepc;
  case CSR_ADDR_M_HARTID:
    return csr->hart_id;
  case CSR_ADDR_M_STATUS:
  case CSR_ADDR_S_STATUS:
    {
      unsigned value = 0;
      value |= (csr->status_sum << 18);
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
#if 0
    fprintf(stderr, "ATP (read) %08x\n", lsu_atp_get(csr->lsu));
#endif
    return lsu_atp_get(csr->lsu);
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
  case CSR_ADDR_M_CYCLE:
  case CSR_ADDR_U_CYCLE:
    return (unsigned)csr->cycle;
  case CSR_ADDR_M_TIME:
  case CSR_ADDR_U_TIME:
    return (unsigned)csr->aclint->mtime;
  case CSR_ADDR_M_INSTRET:
  case CSR_ADDR_U_INSTRET:
    return (unsigned)csr->instret;
  case CSR_ADDR_M_CYCLEH:
  case CSR_ADDR_U_CYCLEH:
    return (unsigned)(csr->cycle >> 32);
  case CSR_ADDR_M_TIMEH:
  case CSR_ADDR_U_TIMEH:
    return (unsigned)(csr->aclint->mtime >> 32);
  case CSR_ADDR_M_INSTRETH:
  case CSR_ADDR_U_INSTRETH:
    return (unsigned)(csr->instret >> 32);
  case CSR_ADDR_M_IP:
    return csr_get_m_interrupts_pending(csr);
  case CSR_ADDR_S_IP:
    return csr_get_s_interrupts_pending(csr);
  case CSR_ADDR_S_EPC:
    return csr->sepc;
  case CSR_ADDR_S_CAUSE:
    return csr->scause;
  case CSR_ADDR_S_TVAL:
    return csr->stval;
  case CSR_ADDR_S_SCRATCH:
    return csr->sscratch;
  case CSR_ADDR_M_CAUSE:
    return csr->mcause;
  case CSR_ADDR_M_TVAL:
    return csr->mtval;
  case CSR_ADDR_M_ISA: {
    unsigned char xml = 0;
    if (XLEN == 32) {
      xml = 1;
    } else if (XLEN == 64) {
      xml = 2;
    } else if (XLEN == 128) {
      xml = 3;
    }
    return
      ((xml << 30) |
       (A_EXTENSION << 0) |
       (C_EXTENSION << 2) |
       (D_EXTENSION << 3) |
       (F_EXTENSION << 5) |
       (M_EXTENSION << 12) |
       (S_EXTENSION << 18) |
       (U_EXTENSION << 20) |
       (V_EXTENSION << 21));
  }
  case CSR_ADDR_D_CSR: {
    unsigned dcsr = 0;
    dcsr =
      (CSR_D_VERSION << 28) |
      (csr->dcsr_ebreakm << 15) |
      (csr->dcsr_ebreaks << 13) |
      (csr->dcsr_ebreaku << 12) |
      (csr->dcsr_cause << 6) |
      (csr->dcsr_mprven << 4) |
      (csr->dcsr_step << 2) |
      (csr->dcsr_prv & 0x3);
    return dcsr;
  }
  case CSR_ADDR_D_PC:
    return csr->dpc;
  case CSR_ADDR_T_SELECT:
    return csr->tselect;
  case CSR_ADDR_T_DATA1:
    return trig_get_tdata(csr->trig, csr->tselect, 0);
  case CSR_ADDR_T_DATA2:
    return trig_get_tdata(csr->trig, csr->tselect, 1);
  case CSR_ADDR_T_DATA3:
    return trig_get_tdata(csr->trig, csr->tselect, 2);
  case CSR_ADDR_T_INFO:
    return trig_info(csr->trig, csr->tselect);
  case CSR_ADDR_M_COUNTEREN:
    return csr->mcounteren;
  case CSR_ADDR_S_COUNTEREN:
    return csr->scounteren;
  case CSR_ADDR_M_VENDORID:
    return VENDOR_ID;
  case CSR_ADDR_M_ARCHID:
    return ARCH_ID;
  case CSR_ADDR_M_IMPID:
    return IMP_ID;
  case CSR_ADDR_M_HPMCOUNTER3:
  case CSR_ADDR_M_HPMCOUNTER4:
  case CSR_ADDR_M_HPMCOUNTER5:
  case CSR_ADDR_M_HPMCOUNTER6:
  case CSR_ADDR_M_HPMCOUNTER7:
  case CSR_ADDR_M_HPMCOUNTER8:
  case CSR_ADDR_M_HPMCOUNTER9:
  case CSR_ADDR_M_HPMCOUNTER10:
  case CSR_ADDR_M_HPMCOUNTER11:
  case CSR_ADDR_M_HPMCOUNTER12:
  case CSR_ADDR_M_HPMCOUNTER13:
  case CSR_ADDR_M_HPMCOUNTER14:
  case CSR_ADDR_M_HPMCOUNTER15:
  case CSR_ADDR_M_HPMCOUNTER16:
  case CSR_ADDR_M_HPMCOUNTER17:
  case CSR_ADDR_M_HPMCOUNTER18:
  case CSR_ADDR_M_HPMCOUNTER19:
  case CSR_ADDR_M_HPMCOUNTER20:
  case CSR_ADDR_M_HPMCOUNTER21:
  case CSR_ADDR_M_HPMCOUNTER22:
  case CSR_ADDR_M_HPMCOUNTER23:
  case CSR_ADDR_M_HPMCOUNTER24:
  case CSR_ADDR_M_HPMCOUNTER25:
  case CSR_ADDR_M_HPMCOUNTER26:
  case CSR_ADDR_M_HPMCOUNTER27:
  case CSR_ADDR_M_HPMCOUNTER28:
  case CSR_ADDR_M_HPMCOUNTER29:
  case CSR_ADDR_M_HPMCOUNTER30:
  case CSR_ADDR_M_HPMCOUNTER31:
    return (unsigned)csr->hpmcounter[addr - CSR_ADDR_M_HPMCOUNTER3];
  case CSR_ADDR_M_HPMCOUNTER3H:
  case CSR_ADDR_M_HPMCOUNTER4H:
  case CSR_ADDR_M_HPMCOUNTER5H:
  case CSR_ADDR_M_HPMCOUNTER6H:
  case CSR_ADDR_M_HPMCOUNTER7H:
  case CSR_ADDR_M_HPMCOUNTER8H:
  case CSR_ADDR_M_HPMCOUNTER9H:
  case CSR_ADDR_M_HPMCOUNTER10H:
  case CSR_ADDR_M_HPMCOUNTER11H:
  case CSR_ADDR_M_HPMCOUNTER12H:
  case CSR_ADDR_M_HPMCOUNTER13H:
  case CSR_ADDR_M_HPMCOUNTER14H:
  case CSR_ADDR_M_HPMCOUNTER15H:
  case CSR_ADDR_M_HPMCOUNTER16H:
  case CSR_ADDR_M_HPMCOUNTER17H:
  case CSR_ADDR_M_HPMCOUNTER18H:
  case CSR_ADDR_M_HPMCOUNTER19H:
  case CSR_ADDR_M_HPMCOUNTER20H:
  case CSR_ADDR_M_HPMCOUNTER21H:
  case CSR_ADDR_M_HPMCOUNTER22H:
  case CSR_ADDR_M_HPMCOUNTER23H:
  case CSR_ADDR_M_HPMCOUNTER24H:
  case CSR_ADDR_M_HPMCOUNTER25H:
  case CSR_ADDR_M_HPMCOUNTER26H:
  case CSR_ADDR_M_HPMCOUNTER27H:
  case CSR_ADDR_M_HPMCOUNTER28H:
  case CSR_ADDR_M_HPMCOUNTER29H:
  case CSR_ADDR_M_HPMCOUNTER30H:
  case CSR_ADDR_M_HPMCOUNTER31H:
    return (unsigned)(csr->hpmcounter[addr - CSR_ADDR_M_HPMCOUNTER3H] >> 32);
  case CSR_ADDR_M_HPMEVENT3:
  case CSR_ADDR_M_HPMEVENT4:
  case CSR_ADDR_M_HPMEVENT5:
  case CSR_ADDR_M_HPMEVENT6:
  case CSR_ADDR_M_HPMEVENT7:
  case CSR_ADDR_M_HPMEVENT8:
  case CSR_ADDR_M_HPMEVENT9:
  case CSR_ADDR_M_HPMEVENT10:
  case CSR_ADDR_M_HPMEVENT11:
  case CSR_ADDR_M_HPMEVENT12:
  case CSR_ADDR_M_HPMEVENT13:
  case CSR_ADDR_M_HPMEVENT14:
  case CSR_ADDR_M_HPMEVENT15:
  case CSR_ADDR_M_HPMEVENT16:
  case CSR_ADDR_M_HPMEVENT17:
  case CSR_ADDR_M_HPMEVENT18:
  case CSR_ADDR_M_HPMEVENT19:
  case CSR_ADDR_M_HPMEVENT20:
  case CSR_ADDR_M_HPMEVENT21:
  case CSR_ADDR_M_HPMEVENT22:
  case CSR_ADDR_M_HPMEVENT23:
  case CSR_ADDR_M_HPMEVENT24:
  case CSR_ADDR_M_HPMEVENT25:
  case CSR_ADDR_M_HPMEVENT26:
  case CSR_ADDR_M_HPMEVENT27:
  case CSR_ADDR_M_HPMEVENT28:
  case CSR_ADDR_M_HPMEVENT29:
  case CSR_ADDR_M_HPMEVENT30:
  case CSR_ADDR_M_HPMEVENT31:
    return csr->hpmevent[addr - CSR_ADDR_M_HPMEVENT3];
  default:
    if (result) result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
#if 0
    fprintf(stderr, "unknown: CSR[R]: addr: %08x @%08x\n", addr, csr->pc);
#endif
    return 0;
  }
}

void csr_csrw(csr_t *csr, unsigned addr, unsigned value, struct core_step_result *result) {
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
    csr->status_sum = (value >> 18) & 0x00000001;
    csr->status_sie = (value >> 1) & 0x00000001;
    csr->status_spie = (value >> 5) & 0x00000001;
    csr->status_spp = (value >> 8) & 0x00000001;
    break;
  case CSR_ADDR_M_HARTID:
    break;
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
    lsu_dcache_write_back(csr->lsu);
    if (value & 0x80000000) {
      lsu_atp_on(csr->lsu, value & 0x000fffff);
    } else {
      lsu_atp_off(csr->lsu);
    }
    lsu_tlb_clear(csr->lsu);
    lsu_icache_invalidate(csr->lsu);
    lsu_dcache_invalidate(csr->lsu);
    result->flush = 1;
#if 0
    fprintf(stderr, "ATP (write) %08x\n", (csr->lsu->vmflag << 31) | ((csr->lsu->vmrppn >> 12) & 0x000fffff));
#endif
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
    if (value & CSR_INT_MSI) {
      csr->aclint->msip = 1;
    } else {
      csr->aclint->msip = 0;
    }
    // fall-through
  case CSR_ADDR_S_IP:
    if (value & CSR_INT_STI) {
      csr->stip = 1;
    } else {
      csr->stip = 0;
    }
    if (value & CSR_INT_SSI) {
      csr->aclint->ssip = 1;
    } else {
      csr->aclint->ssip = 0;
    }
    break;
  case CSR_ADDR_S_TVAL:
    csr->stval = value;
    break;
  case CSR_ADDR_S_SCRATCH:
    csr->sscratch = value;
    break;
  case CSR_ADDR_M_CAUSE:
    csr->mcause = value;
    break;
  case CSR_ADDR_M_TVAL:
    csr->mtval = value;
    break;
  case CSR_ADDR_M_ISA:
    break;
  case CSR_ADDR_M_CYCLE:
    csr->cycle = (csr->cycle & 0xffffffff00000000) | (unsigned)value;
    break;
  case CSR_ADDR_M_TIME:
    csr->aclint->mtime = (csr->aclint->mtime & 0xffffffff00000000) | (unsigned)value;
    break;
  case CSR_ADDR_M_INSTRET:
    csr->instret = (csr->instret & 0xffffffff00000000) | (unsigned)value;
    break;
  case CSR_ADDR_M_CYCLEH:
    csr->cycle = (csr->cycle & 0x00000000ffffffff) | (unsigned long long)value << 32;
    break;
  case CSR_ADDR_M_TIMEH:
    csr->aclint->mtime = (csr->aclint->mtime & 0x00000000ffffffff) | (unsigned long long)value << 32;
    break;
  case CSR_ADDR_M_INSTRETH:
    csr->instret = (csr->instret & 0x00000000ffffffff) | (unsigned long long)value << 32;
    break;
  case CSR_ADDR_D_CSR:
    csr->dcsr_ebreakm = (value >> 15) & 0x1;
    csr->dcsr_ebreaks = (value >> 13) & 0x1;
    csr->dcsr_ebreaku = (value >> 12) & 0x1;
    csr->dcsr_mprven = (value >> 4) & 0x1;
    csr->dcsr_step = (value >> 2) & 0x1;
    csr->dcsr_prv = value & 0x3;
    break;
  case CSR_ADDR_D_PC:
    csr->dpc = value;
    break;
  case CSR_ADDR_T_SELECT:
    csr->tselect = value;
    if (value + 1 > csr->trig->size) {
      trig_resize(csr->trig, value + 1);
    }
    break;
  case CSR_ADDR_T_DATA1:
    trig_set_tdata(csr->trig, csr->tselect, 0, value);
    break;
  case CSR_ADDR_T_DATA2:
    trig_set_tdata(csr->trig, csr->tselect, 1, value);
    break;
  case CSR_ADDR_T_DATA3:
    trig_set_tdata(csr->trig, csr->tselect, 2, value);
    break;
  case CSR_ADDR_M_COUNTEREN:
    csr->mcounteren = value & 0x07;
    break;
  case CSR_ADDR_S_COUNTEREN:
    csr->scounteren = value & 0x07;
    break;
  case CSR_ADDR_M_VENDORID:
  case CSR_ADDR_M_ARCHID:
  case CSR_ADDR_M_IMPID:
    break;
  case CSR_ADDR_M_HPMCOUNTER3:
  case CSR_ADDR_M_HPMCOUNTER4:
  case CSR_ADDR_M_HPMCOUNTER5:
  case CSR_ADDR_M_HPMCOUNTER6:
  case CSR_ADDR_M_HPMCOUNTER7:
  case CSR_ADDR_M_HPMCOUNTER8:
  case CSR_ADDR_M_HPMCOUNTER9:
  case CSR_ADDR_M_HPMCOUNTER10:
  case CSR_ADDR_M_HPMCOUNTER11:
  case CSR_ADDR_M_HPMCOUNTER12:
  case CSR_ADDR_M_HPMCOUNTER13:
  case CSR_ADDR_M_HPMCOUNTER14:
  case CSR_ADDR_M_HPMCOUNTER15:
  case CSR_ADDR_M_HPMCOUNTER16:
  case CSR_ADDR_M_HPMCOUNTER17:
  case CSR_ADDR_M_HPMCOUNTER18:
  case CSR_ADDR_M_HPMCOUNTER19:
  case CSR_ADDR_M_HPMCOUNTER20:
  case CSR_ADDR_M_HPMCOUNTER21:
  case CSR_ADDR_M_HPMCOUNTER22:
  case CSR_ADDR_M_HPMCOUNTER23:
  case CSR_ADDR_M_HPMCOUNTER24:
  case CSR_ADDR_M_HPMCOUNTER25:
  case CSR_ADDR_M_HPMCOUNTER26:
  case CSR_ADDR_M_HPMCOUNTER27:
  case CSR_ADDR_M_HPMCOUNTER28:
  case CSR_ADDR_M_HPMCOUNTER29:
  case CSR_ADDR_M_HPMCOUNTER30:
  case CSR_ADDR_M_HPMCOUNTER31:
    csr->hpmcounter[addr - CSR_ADDR_M_HPMCOUNTER3] =
      (csr->hpmcounter[addr - CSR_ADDR_M_HPMCOUNTER3] & 0xffffffff00000000) |
      (unsigned)value;
    break;
  case CSR_ADDR_M_HPMCOUNTER3H:
  case CSR_ADDR_M_HPMCOUNTER4H:
  case CSR_ADDR_M_HPMCOUNTER5H:
  case CSR_ADDR_M_HPMCOUNTER6H:
  case CSR_ADDR_M_HPMCOUNTER7H:
  case CSR_ADDR_M_HPMCOUNTER8H:
  case CSR_ADDR_M_HPMCOUNTER9H:
  case CSR_ADDR_M_HPMCOUNTER10H:
  case CSR_ADDR_M_HPMCOUNTER11H:
  case CSR_ADDR_M_HPMCOUNTER12H:
  case CSR_ADDR_M_HPMCOUNTER13H:
  case CSR_ADDR_M_HPMCOUNTER14H:
  case CSR_ADDR_M_HPMCOUNTER15H:
  case CSR_ADDR_M_HPMCOUNTER16H:
  case CSR_ADDR_M_HPMCOUNTER17H:
  case CSR_ADDR_M_HPMCOUNTER18H:
  case CSR_ADDR_M_HPMCOUNTER19H:
  case CSR_ADDR_M_HPMCOUNTER20H:
  case CSR_ADDR_M_HPMCOUNTER21H:
  case CSR_ADDR_M_HPMCOUNTER22H:
  case CSR_ADDR_M_HPMCOUNTER23H:
  case CSR_ADDR_M_HPMCOUNTER24H:
  case CSR_ADDR_M_HPMCOUNTER25H:
  case CSR_ADDR_M_HPMCOUNTER26H:
  case CSR_ADDR_M_HPMCOUNTER27H:
  case CSR_ADDR_M_HPMCOUNTER28H:
  case CSR_ADDR_M_HPMCOUNTER29H:
  case CSR_ADDR_M_HPMCOUNTER30H:
  case CSR_ADDR_M_HPMCOUNTER31H:
    csr->hpmcounter[addr - CSR_ADDR_M_HPMCOUNTER3H] =
      (csr->hpmcounter[addr - CSR_ADDR_M_HPMCOUNTER3H] & 0x00000000ffffffff) |
      (unsigned long long)value << 32;
    break;
  case CSR_ADDR_M_HPMEVENT3:
  case CSR_ADDR_M_HPMEVENT4:
  case CSR_ADDR_M_HPMEVENT5:
  case CSR_ADDR_M_HPMEVENT6:
  case CSR_ADDR_M_HPMEVENT7:
  case CSR_ADDR_M_HPMEVENT8:
  case CSR_ADDR_M_HPMEVENT9:
  case CSR_ADDR_M_HPMEVENT10:
  case CSR_ADDR_M_HPMEVENT11:
  case CSR_ADDR_M_HPMEVENT12:
  case CSR_ADDR_M_HPMEVENT13:
  case CSR_ADDR_M_HPMEVENT14:
  case CSR_ADDR_M_HPMEVENT15:
  case CSR_ADDR_M_HPMEVENT16:
  case CSR_ADDR_M_HPMEVENT17:
  case CSR_ADDR_M_HPMEVENT18:
  case CSR_ADDR_M_HPMEVENT19:
  case CSR_ADDR_M_HPMEVENT20:
  case CSR_ADDR_M_HPMEVENT21:
  case CSR_ADDR_M_HPMEVENT22:
  case CSR_ADDR_M_HPMEVENT23:
  case CSR_ADDR_M_HPMEVENT24:
  case CSR_ADDR_M_HPMEVENT25:
  case CSR_ADDR_M_HPMEVENT26:
  case CSR_ADDR_M_HPMEVENT27:
  case CSR_ADDR_M_HPMEVENT28:
  case CSR_ADDR_M_HPMEVENT29:
  case CSR_ADDR_M_HPMEVENT30:
  case CSR_ADDR_M_HPMEVENT31:
    csr->hpmevent[addr - CSR_ADDR_M_HPMEVENT3] = value;
    break;
  default:
    if (result) result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
#if 0
    fprintf(stderr, "unknown: CSR[W]: addr: %08x value: %08x @%08x\n", addr, value, csr->pc);
#endif
    break;
  }
  return;
}

unsigned csr_csrrw(csr_t *csr, unsigned addr, unsigned value, struct core_step_result *result) {
  unsigned csr_read_value = 0;
  if (result->rd_regno != 0) {
    // if rd = x0, any CSR will not be read.
    csr_read_value = csr_csrr(csr, addr, result);
  }
  csr_csrw(csr, addr, value, result);
  return csr_read_value;
}

unsigned csr_csrrs(csr_t *csr, unsigned addr, unsigned value, struct core_step_result *result) {
  unsigned csr_read_value = csr_csrr(csr, addr, result);
  if (!(result->rs1_regno == 0 && value == 0)) {
    // if rs1_regno = x0 (CSRRS) "OR" value = 0 (CSRRSI), any CSR will be written.
    csr_csrw(csr, addr, value | csr_read_value, result);
  }
  return csr_read_value;
}

unsigned csr_csrrc(csr_t *csr, unsigned addr, unsigned value, struct core_step_result *result) {
  unsigned csr_read_value = csr_csrr(csr, addr, result);
  if (!(result->rs1_regno == 0 && value == 0)) {
    // if rs1_regno = x0 (CSRRC) "OR" value = 0 (CSRRCI), any CSR will be written.
    csr_csrw(csr, addr, (~value) & csr_read_value, result);
  }
  return csr_read_value;
}

static void csr_enter_debug_mode(csr_t *csr, unsigned cause) {
  csr->dpc = csr->pc;
  csr->dcsr_prv = csr->mode;
  csr->dcsr_cause = cause;
  csr->mode = PRIVILEGE_MODE_D;
}

static void csr_trap(csr_t *csr, unsigned trap_code, unsigned trap_value) {
  unsigned is_interrupt = ((trap_code >> 31) & 0x00000001);
  unsigned code = (trap_code & 0x7fffffff);
  // default: to Machine Mode
  unsigned to_mode = PRIVILEGE_MODE_M;
  // delegation check
  if (csr->dcsr_step) {
    to_mode = PRIVILEGE_MODE_D;
  } else if (is_interrupt) {
    if (
        // (a) if either the current privilege mode is M and the MIE bit in the mstatus register is set, or the current privilege mode has less privilege than M-mode
        ((csr->mode == PRIVILEGE_MODE_M && csr->status_mie) ||
         (csr->mode < PRIVILEGE_MODE_M)) &&
        // (b) interrupt bit is set in both mip and mie
        ((((csr_get_m_interrupts_pending(csr) & csr->interrupts_enable) >> code)) & 0x00000001) &&
        // (c) interrupt is not set in mideleg
        !((csr->mideleg >> code) & 0x00000001)
        ) {
      to_mode = PRIVILEGE_MODE_M;
    } else {
      to_mode = PRIVILEGE_MODE_S;
    }
  } else {
    if ((trap_code == TRAP_CODE_BREAKPOINT) &&
        ((csr->mode == PRIVILEGE_MODE_M && csr->dcsr_ebreakm) ||
         (csr->mode == PRIVILEGE_MODE_S && csr->dcsr_ebreaks) ||
         (csr->mode == PRIVILEGE_MODE_U && csr->dcsr_ebreaku))
        ) {
      // entre debug mode
      to_mode = PRIVILEGE_MODE_D;
    } else if (
        // To S mode if current privilege mode is less than M and medeleg is set
        (csr->mode < PRIVILEGE_MODE_M) &&
        (((csr->medeleg >> code)) & 0x00000001)
        ) {
      to_mode = PRIVILEGE_MODE_S;
    } else {
      to_mode = PRIVILEGE_MODE_M;
    }
  }

  if (to_mode == PRIVILEGE_MODE_D) {
    if (csr->dcsr_step) {
      csr_enter_debug_mode(csr, CSR_DCSR_CAUSE_STEP);
    } else {
      csr_enter_debug_mode(csr, CSR_DCSR_CAUSE_EBREAK);
    }
  } else if (to_mode == PRIVILEGE_MODE_M) {
    csr->mtval = trap_value;
    csr->mcause = trap_code;
    // pc
    csr->mepc = csr->pc;
    csr->pc = csr->mtvec;
    // enable
    csr->status_mpie = csr->status_mie;
    csr->status_mie = 0;
    // mode
    csr->status_mpp = csr->mode;
    csr->mode = to_mode;
#if 0
    fprintf(stderr, "[to M] trap from %d to %u trap_code %08x epc %08x next_pc %08x\n", csr->status_mpp, to_mode, trap_code, csr->mepc, csr->pc);
    fprintf(stderr, "MIDELEG %08x MEDELEG %08x\n", csr->mideleg, csr->medeleg);
#endif
  } else if (to_mode == PRIVILEGE_MODE_S) {
    csr->stval = trap_value;
    csr->scause = trap_code;
    // pc
    csr->sepc = csr->pc;
    csr->pc = csr->stvec;
    // enable
    csr->status_spie = csr->status_sie;
    csr->status_sie = 0;
    // mode
    csr->status_spp = csr->mode;
    csr->mode = to_mode;
#if 0
    fprintf(stderr, "[to S] trap from %d to %u trap_code %08x epc %08x next_pc %08x\n", csr->status_spp, to_mode, trap_code, csr->sepc, csr->pc);
    fprintf(stderr, "MIDELEG %08x MEDELEG %08x\n", csr->mideleg, csr->medeleg);
#endif
  }
  return;
}

static void csr_restore_trap(csr_t *csr) {
  unsigned from_mode = csr->mode;
  lsu_tlb_clear(csr->lsu);
  lsu_icache_invalidate(csr->lsu);
  lsu_dcache_invalidate(csr->lsu);
  if (from_mode == PRIVILEGE_MODE_M) {
    // pc
    csr->pc = csr->mepc;
    csr->mepc = 0;
    // enable
    csr->status_mie = csr->status_mpie;
    csr->status_mpie = 1;
    // mode
    csr->mode = csr->status_mpp;
    csr->status_mpp = 0;
  } else {
    // pc
    csr->pc = csr->sepc;
    csr->sepc = 0;
    // enable
    csr->status_sie = csr->status_spie;
    csr->status_spie = 1;
    // mode
    csr->mode = csr->status_spp;
    csr->status_spp = 0;
  }
}

static void csr_update_counters(csr_t *csr, struct core_step_result *result) {
  csr->cycle++; // assume 100 MHz
  csr->instret++;
  csr->pc = result->pc_next;
  // TODO: variable counting
  // HPM3 regread total
  // HPM4 regread skip total
  // HPM5 regwrite total
  // HPM6 regwrite skip total
  // HPM7 ICACHE access
  // HPM8 ICACHE hit
  // HPM9 DCACHE access
  // HPM10 DCACHE hit
  // HPM11 TLB access
  // HPM12 TLB hit
  if (csr->regstat_en) {
    if (result->opcode == OPCODE_OP || result->opcode == OPCODE_OP_IMM) {
      unsigned r_break = 0;
      unsigned w_break = 0;
      for (int i = 1; i < 4; i++) {
        // read skip search
        int r_index = result->inst_window_pos - i;
        if (!r_break && r_index < CORE_WINDOW_SIZE && result->inst_window_pc[r_index] != 0xffffffff) {
          unsigned r_inst = riscv_decompress(result->inst_window[r_index]);
          unsigned r_opcode = riscv_get_opcode(r_inst);
          if (r_opcode == OPCODE_OP || r_opcode == OPCODE_OP_IMM ||
              r_opcode == OPCODE_LUI || r_opcode == OPCODE_AUIPC) {
            if (result->rs1_regno != 0 && result->rs1_regno == riscv_get_rd(r_inst)) {
              result->rs1_read_skip = 1;
            }
            if (result->opcode == OPCODE_OP && result->rs2_regno != 0 &&
                result->rs2_regno == riscv_get_rd(r_inst)) {
              result->rs2_read_skip = 1;
            }
          } else if (r_opcode == OPCODE_BRANCH || r_opcode == OPCODE_JAL || r_opcode == OPCODE_JALR) {
            r_break = 1;
          }
        }
        // write skip search
        int w_index = result->inst_window_pos + i;
        if (!w_break && w_index >= 0 && result->inst_window_pc[w_index] != 0xffffffff) {
          unsigned w_inst = riscv_decompress(result->inst_window[w_index]);
          unsigned w_opcode = riscv_get_opcode(w_inst);
          if (w_opcode == OPCODE_OP || w_opcode == OPCODE_OP_IMM ||
              w_opcode == OPCODE_LUI || w_opcode == OPCODE_AUIPC || w_opcode == OPCODE_LOAD) {
            if (result->opcode == OPCODE_OP && result->rd_regno != 0 &&
                result->rd_regno == riscv_get_rd(w_inst)) {
              result->rd_write_skip = 1;
            }
          } else if (w_opcode == OPCODE_BRANCH || w_opcode == OPCODE_JAL || w_opcode == OPCODE_JALR) {
            w_break = 1;
          }
        }
      }
    }
  }
  if (result->rs1_regno != 0) {
    csr->hpmcounter[0]++;
  }
  if (result->rs1_read_skip != 0) {
    csr->hpmcounter[1]++;
  }
  if (result->rs2_regno != 0) {
    csr->hpmcounter[0]++;
  }
  if (result->rs2_read_skip != 0) {
    csr->hpmcounter[1]++;
  }
  if (result->rd_regno != 0) {
    csr->hpmcounter[2]++;
  }
  if (result->rd_write_skip != 0) {
    csr->hpmcounter[3]++;
  }
  if (result->icache_access) {
    csr->hpmcounter[4]++;
  }
  if (result->icache_hit) {
    csr->hpmcounter[5]++;
  }
  if (result->dcache_access) {
    csr->hpmcounter[6]++;
  }
  if (result->dcache_hit) {
    csr->hpmcounter[7]++;
  }
  if (result->tlb_access) {
    csr->hpmcounter[8]++;
  }
  if (result->tlb_hit) {
    csr->hpmcounter[9]++;
  }
}

void csr_cycle(csr_t *csr, struct core_step_result *result) {
  // update counters
  csr_update_counters(csr, result);

  if (result->trigger) {
    // [TODO] trigger timing control
    csr_enter_debug_mode(csr, CSR_DCSR_CAUSE_TRIGGER);
  } else if (result->trapret) {
    csr_restore_trap(csr);
  } else if (result->exception_code != 0 || csr->dcsr_step) {
    // catch exception
    if ((result->exception_code == TRAP_CODE_LOAD_PAGE_FAULT) ||
        (result->exception_code == TRAP_CODE_LOAD_ACCESS_FAULT) ||
        (result->exception_code == TRAP_CODE_STORE_PAGE_FAULT) ||
        (result->exception_code == TRAP_CODE_STORE_ACCESS_FAULT)) {
      csr_trap(csr, result->exception_code, result->m_vaddr);
    } else if ((result->exception_code == TRAP_CODE_INSTRUCTION_PAGE_FAULT) ||
               (result->exception_code == TRAP_CODE_INSTRUCTION_ACCESS_FAULT)) {
      csr_trap(csr, result->exception_code, result->pc);
    } else {
      csr_trap(csr, result->exception_code, 0);
    }
  } else if ((csr->cycle % 10) == 0) {
    // catch interrupt
    unsigned interrupts_enable;
    if (csr->mode == PRIVILEGE_MODE_M) {
      interrupts_enable = (csr->status_mie) ? csr->interrupts_enable : 0;
    } else if (csr->mode == PRIVILEGE_MODE_S) {
      interrupts_enable = (csr->status_sie) ? csr->interrupts_enable : 0;
    } else {
      // all interrupts are enabled
      interrupts_enable = 0x0000FFFF;
    }

    unsigned interrupts_pending = csr_get_m_interrupts_pending(csr);
    unsigned interrupt = interrupts_enable & interrupts_pending;
    // Simultaneous interrupts destined for M-mode are handled in the following
    // decreasing priority order: MEI, MSI, MTI, SEI, SSI, STI
    if (interrupt & CSR_INT_MEI) {
      csr_trap(csr, TRAP_CODE_M_EXTERNAL_INTERRUPT, 0);
    } else if (interrupt & CSR_INT_MSI) {
      csr_trap(csr, TRAP_CODE_M_SOFTWARE_INTERRUPT, 0);
    } else if (interrupt & CSR_INT_MTI) {
      csr_trap(csr, TRAP_CODE_M_TIMER_INTERRUPT, 0);
    } else if (interrupt & CSR_INT_SEI) {
      csr_trap(csr, TRAP_CODE_S_EXTERNAL_INTERRUPT, 0);
    } else if (interrupt & CSR_INT_SSI) {
      csr_trap(csr, TRAP_CODE_S_SOFTWARE_INTERRUPT, 0);
    } else if (interrupt & CSR_INT_STI) {
      csr_trap(csr, TRAP_CODE_S_TIMER_INTERRUPT, 0);
    } else {
      // unknown
    }
#if 0
    if (interrupt) {
      fprintf(stderr, "mode %d interrupts_pending %08x interrupts_enable %08x SIE %08x MIE %08x\n",
              csr->mode,
              interrupts_pending,
              interrupts_enable,
              csr->status_mie,
              csr->status_sie);
      fprintf(stderr, "%016lx, %016lx\n", csr->time, csr->timecmp);
    }
#endif
  }
  return;
}

void csr_fini(csr_t *csr) {
  return;
}
