#ifndef CSR_H
#define CSR_H

struct core_step_result;
struct memory_t;
struct plic_t;
struct aclint_t;
struct trigger_t;

typedef struct csr_t {
  struct lsu_t *lsu;
  struct plic_t *plic;
  struct aclint_t *aclint;
  struct trigger_t *trig;
  // shadow registers
  unsigned mode;
  unsigned pc;
  unsigned hart_id;
  unsigned long long cycle;
  unsigned long long instret;
  unsigned char stip;
  unsigned char status_spp; // previous privilege mode
  unsigned char status_mpp; // previous privilege mode
  unsigned status_sie; // global interrupt enable
  unsigned status_mie; // global interrupt enable
  unsigned status_spie; // global previous interrupt enable
  unsigned status_mpie; // global previous interrupt enable
  unsigned char status_sum; // supervisor-mode can access usermode memory
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
  unsigned mcounteren;
  unsigned scounteren;
  unsigned long long hpmcounter[29];
  unsigned hpmevent[29];
  // statistics (extension)
  unsigned char regstat_en; // for register access analysis
  // Sdext
  unsigned char dcsr_ebreakm;
  unsigned char dcsr_ebreaks;
  unsigned char dcsr_ebreaku;
  unsigned char dcsr_cause;
  unsigned char dcsr_step;
  unsigned char dcsr_mprven;
  unsigned char dcsr_prv; // previous privilege mode
  unsigned dpc; // debugging PC
  // Sdtrig
  unsigned tselect;
} csr_t;

void csr_init(csr_t *);
void csr_fini(csr_t *);
// call once at every cycle
void csr_cycle(csr_t *, struct core_step_result *);
// basic interface
unsigned csr_csrr(csr_t *, unsigned addr, struct core_step_result *result);
void csr_csrw(csr_t *, unsigned addr, unsigned value, struct core_step_result *result);
unsigned csr_csrrw(csr_t *, unsigned addr, unsigned value, struct core_step_result *result);
unsigned csr_csrrs(csr_t *, unsigned addr, unsigned value, struct core_step_result *result);
unsigned csr_csrrc(csr_t *, unsigned addr, unsigned value, struct core_step_result *result);

#endif
