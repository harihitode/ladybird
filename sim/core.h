#ifndef CORE_H
#define CORE_H

#include "sim.h"

struct csr_t;
struct memory_t;
struct plic_t;
struct aclint_t;
struct trigger_t;

typedef struct window_t {
  unsigned *pc;
  unsigned *pc_paddr;
  unsigned *inst;
  unsigned *exception;
} window_t;

typedef struct core_t {
  unsigned gpr[NUM_GPR];
#if F_EXTENSION
  unsigned fpr[NUM_FPR];
#endif
  struct csr_t *csr;
  struct lsu_t *lsu;
  window_t window; // instruction window
} core_t;

void core_init(core_t *, int hart_id, struct memory_t *, struct plic_t *, struct aclint_t *, struct trigger_t *);
void core_step(core_t *core, unsigned pc, struct core_step_result *result, unsigned prv);
void core_window_flush(core_t *core);
void core_fini(core_t *core);

#endif
