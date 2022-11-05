#ifndef GDBSTUB_SYS_H
#define GDBSTUB_SYS_H

#include "riscv.h"
#include "sim.h"

#define DBG_CPU_NUM_REGISTERS NUM_REGISTERS

typedef unsigned int address;

struct breakpoint {
  int valid;
  address addr;
  address inst;
};

struct dbg_state {
  sim_t *sim;
  int sock, client;
  int n_bp;
  struct breakpoint *bp;
};

#endif
