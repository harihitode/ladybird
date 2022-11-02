#ifndef GDBSTUB_SYS_H
#define GDBSTUB_SYS_H

#include "riscv.h"

struct memory_t;
struct aclint_t;
struct csr_t;
struct elf_t;
struct core_t;

#define CONFIG_ROM_ADDR 0x00001000
#define CONFIG_ROM_SIZE 1024
#define DBG_CPU_NUM_REGISTERS NUM_REGISTERS

typedef unsigned int address;

struct dbg_trigger {
  unsigned index;
  address addr;
  int type;
  int kind;
  unsigned value;
  struct dbg_trigger *next;
};

enum memory_access { MA_NONE, MA_LOAD, MA_STORE };

struct dbg_step_result {
  unsigned prv;
  unsigned inst;
  unsigned rd_regno;
  unsigned rd_data;
  unsigned pc_next;
  unsigned exception_code;
  enum memory_access mem_access;
  unsigned mem_virtual_addr;
  unsigned trapret;
  unsigned trigger;
};

enum dbg_state_state { running, quit };

struct dbg_state {
  enum dbg_state_state state;
  struct core_t *core;
  struct memory_t *mem;
  struct csr_t *csr;
  struct elf_t *elf;
  struct dbg_trigger *trigger;
  struct uart_t *uart;
  struct disk_t *disk;
  struct plic_t *plic;
  struct aclint_t *aclint;
  unsigned dbg_mode;
  char **reginfo;  // register information
  char triple[64]; // triple information
  char *config_rom;
  void (*dbg_handler)(struct dbg_state *);
};

#endif
