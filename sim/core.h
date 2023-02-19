#ifndef CORE_H
#define CORE_H

#include "sim.h"

struct csr_t;
struct memory_t;

typedef struct window_t {
  unsigned *pc;
  unsigned *inst;
  unsigned *exception;
} window_t;

typedef struct core_t {
  unsigned gpr[NUM_GPR];
  struct csr_t *csr;
  struct memory_t *mem;
  window_t window; // instruction window
} core_t;

void core_init(core_t *core);
void core_step(core_t *core, unsigned pc, struct core_step_result *result, unsigned prv);
void core_window_flush(core_t *core);
void core_fini(core_t *core);

#endif
