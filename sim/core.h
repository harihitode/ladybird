#ifndef CORE_H
#define CORE_H

#include "sim.h"

struct csr_t;
struct memory_t;

enum memory_access { MA_NONE, MA_LOAD, MA_STORE };

struct step_result {
  unsigned rd;
  unsigned rd_data;
  enum memory_access mem_access;
  unsigned mem_address;
  unsigned pc_next;
};

typedef struct core_t {
  unsigned gpr[NUM_GPR];
  struct csr_t *csr;
  struct memory_t *mem;
} core_t;

void core_init(core_t *core);
void core_step(core_t *core, unsigned pc, struct step_result *result);
void core_fini(core_t *core);

#endif
