#ifndef GDBSTUB_SYS_H
#define GDBSTUB_SYS_H

#define DBG_CPU_NUM_REGISTERS NUM_REGISTERS

struct memory_t;
struct csr_t;
struct elf_t;

#define NUM_GPR 32
#ifdef F_EXTENSION
#define NUM_FPR 32
#else
#define NUM_FPR 0
#endif
#define NUM_REGISTERS (NUM_GPR + 1 + NUM_FPR) // 1 is for PC
#define REG_PC 32

typedef unsigned int address;

struct dbg_break_watch {
  address addr;
  int type;
  int kind;
  unsigned value;
  struct dbg_break_watch *next;
};

typedef struct dbg_state {
  unsigned *registers;
  struct memory_t *mem;
  struct csr_t *csr;
  struct elf_t *elf;
  // for debugger stub
  unsigned dbg_mode; // 1:stab mode
  unsigned signum; // trap cause number
  char **reginfo;  // register information
  char triple[64]; // triple information
  struct dbg_break_watch *bw; // break and watch point
} sim_t;

#endif
