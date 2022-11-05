#ifndef CORE_H
#define CORE_H

#include "sim.h"

struct core_step_result {
  unsigned char prv;
  unsigned inst;
  unsigned rd_regno;
  unsigned rd_data;
  unsigned pc_next;
  unsigned exception_code;
  unsigned char m_access;
  unsigned m_vaddr;
  unsigned m_data;
  unsigned char trapret;
  unsigned char trigger;
};

struct csr_t;
struct memory_t;

typedef struct core_t {
  unsigned gpr[NUM_GPR];
  struct csr_t *csr;
  struct memory_t *mem;
} core_t;

void core_init(core_t *core);
void core_step(core_t *core, unsigned pc, struct core_step_result *result, unsigned prv);
void core_fini(core_t *core);

#endif
