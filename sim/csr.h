#ifndef CSR_H
#define CSR_H

#include <stdint.h>

struct dbg_state;

typedef struct csr_t {
  struct dbg_state *sim;
  void (*trap_handler)(struct dbg_state *);
  uint64_t cycle;
  uint64_t time;
  uint64_t timecmp;
  uint64_t instret;;
  unsigned mode;
  unsigned software_interrupt_m;
  unsigned software_interrupt_s;
  unsigned status_spp; // previous privilege mode
  unsigned status_mpp; // previous privilege mode
  unsigned status_sie; // global interrupt enable
  unsigned status_mie; // global interrupt enable
  unsigned status_spie; // global previous interrupt enable
  unsigned status_mpie; // global previous interrupt enable
  unsigned hartid;
  unsigned trapret;
  unsigned interrupt;
  unsigned exception;
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
} csr_t;

void csr_init(csr_t *);
void csr_set_sim(csr_t *, struct dbg_state *);
unsigned csr_csrr(csr_t *, unsigned addr);
void csr_csrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrs(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrc(csr_t *, unsigned addr, unsigned value);
// trap
void csr_trap(csr_t *, unsigned trap_code);
void csr_trapret(csr_t *);
void csr_restore_trap(csr_t *);
// exception
void csr_exception(csr_t *, unsigned trap_code);
// timer
uint64_t csr_get_timecmp(csr_t *);
void csr_set_timecmp(csr_t *, uint64_t);
void csr_fini(csr_t *);
// call once for 1 cycle
void csr_cycle(csr_t *, int is_instret);

#endif
