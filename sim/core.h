#ifndef CORE_H
#define CORE_H

#include "sim.h"

struct csr_t;
struct memory_t;

typedef struct core_t {
  unsigned gpr[NUM_GPR];
  struct csr_t *csr;
  struct memory_t *mem;
} core_t;

void core_init(core_t *core);
void core_step(core_t *core, unsigned pc, struct dbg_step_result *result, unsigned prv);
void core_fini(core_t *core);

#endif
