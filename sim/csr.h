#ifndef CSR_H
#define CSR_H

struct dbg_step_result;
struct memory_t;
struct plic_t;

typedef struct csr_t {
  struct memory_t *mem;
  struct plic_t *plic;
  // shadow registers
  unsigned mode;
  unsigned pc;
  // status
  unsigned long long cycle;
  unsigned long long time;
  unsigned long long timecmp;
  unsigned long long instret;
  unsigned software_interrupt_m;
  unsigned software_interrupt_s;
  unsigned char status_spp; // previous privilege mode
  unsigned char status_mpp; // previous privilege mode
  unsigned status_sie; // global interrupt enable
  unsigned status_mie; // global interrupt enable
  unsigned status_spie; // global previous interrupt enable
  unsigned status_mpie; // global previous interrupt enable
  unsigned hartid;
  unsigned trapret;
  unsigned interrupt;
  unsigned exception_code;
  unsigned interrupts_enable;
  unsigned mideleg;
  unsigned medeleg;
  unsigned mepc;
  unsigned mcause;
  unsigned mscratch;
  unsigned mtval; // [M] trap value
  unsigned mtvec; // [M] trap base address
  unsigned sepc;
  unsigned scause;
  unsigned sscratch;
  unsigned stvec; // [S] trap base address
  unsigned stval; // [S] trap value
  // Sdext
  unsigned char dcsr_ebreakm;
  unsigned char dcsr_ebreaks;
  unsigned char dcsr_ebreaku;
  unsigned char dcsr_cause;
  unsigned char dcsr_step;
  unsigned char dcsr_prv; // previous privilege mode
  unsigned dpc; // debugging PC
} csr_t;

void csr_init(csr_t *);
unsigned csr_csrr(csr_t *, unsigned addr);
void csr_csrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrs(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrc(csr_t *, unsigned addr, unsigned value);
// trap
void csr_trap(csr_t *, unsigned trap_code);
void csr_trapret(csr_t *);
void csr_restore_trap(csr_t *);
// timer
unsigned long long csr_get_timecmp(csr_t *);
void csr_set_timecmp(csr_t *, unsigned long long);
void csr_fini(csr_t *);
// call once for 1 cycle
void csr_cycle(csr_t *, struct dbg_step_result *);

#endif
