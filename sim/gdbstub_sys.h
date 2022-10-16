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

typedef struct dbg_state {
  unsigned signum;
  unsigned *registers;
  unsigned dbg_mode;
  char **reginfo;
  struct memory_t *mem;
  struct csr_t *csr;
  struct elf_t *elf;
} sim_t;

typedef unsigned int address;

#endif
