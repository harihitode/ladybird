#ifndef GDBSTUB_SYS_H
#define GDBSTUB_SYS_H

#define DBG_CPU_NUM_REGISTERS NUM_REGISTERS

struct memory_t;
struct csr_t;
struct elf_t;
struct core_t;

#define NUM_GPR 32
#ifdef F_EXTENSION
#define NUM_FPR 32
#else
#define NUM_FPR 0
#endif
#define NUM_REGISTERS (NUM_GPR + 1 + NUM_FPR) // 1 is for PC

#define CONFIG_ROM_ADDR 0x00001000
#define CONFIG_ROM_SIZE 1024

typedef unsigned int address;

struct dbg_break_watch {
  address addr;
  int type;
  int kind;
  unsigned value;
  struct dbg_break_watch *next;
};

struct dbg_state {
  struct core_t *core;
  struct memory_t *mem;
  struct csr_t *csr;
  struct elf_t *elf;
  unsigned dbg_mode;
  char **reginfo;  // register information
  char triple[64]; // triple information
  struct dbg_break_watch *bw; // break and watch point
  char *config_rom;
  void (*dbg_handler)(struct dbg_state *);
};

#endif
