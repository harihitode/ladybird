#ifndef SIM_H
#define SIM_H

#include <stdio.h>

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

typedef struct sim_t {
  unsigned signum;
  unsigned *registers;
  struct memory_t *mem;
  struct csr_t *csr;
  struct elf_t *elf;
} sim_t;

void sim_init(sim_t *);
void sim_step(sim_t *);
unsigned sim_read_register(sim_t *, unsigned regno);
void sim_write_register(sim_t *, unsigned regno, unsigned value);
unsigned sim_read_memory(sim_t *, unsigned addr);
void sim_write_memory(sim_t *, unsigned addr, unsigned value);
void sim_fini(sim_t *);
// trap
// set callback function when exception occurs
void sim_trap(sim_t *, void (*func)(sim_t *sim));
unsigned sim_get_trap_code(sim_t *);
unsigned sim_get_trap_value(sim_t *);
unsigned sim_get_epc(sim_t *);
unsigned sim_get_instruction(sim_t *, unsigned pc);
// mmio
int sim_load_elf(sim_t *, const char *elf_path);
int sim_virtio_disk(sim_t *, const char *img_path, int mode);
int sim_uart_io(sim_t *, FILE *in, FILE *out);
// debug
void sim_debug_dump_status(sim_t *);

// trap code below
#define TRAP_CODE_ILLEGAL_INSTRUCTION 0x00000002
#define TRAP_CODE_ENVIRONMENT_CALL_M 0x0000000b
#define TRAP_CODE_ENVIRONMENT_CALL_S 0x00000009
#define TRAP_CODE_BREAKPOINT 0x00000003
#define TRAP_CODE_LOAD_ACCESS_FAULT 0x00000005
#define TRAP_CODE_STORE_ACCESS_FAULT 0x00000007
#define TRAP_CODE_M_EXTERNAL_INTERRUPT 0x8000000b
#define TRAP_CODE_S_EXTERNAL_INTERRUPT 0x80000009
#define TRAP_CODE_M_TIMER_INTERRUPT 0x80000007
#define TRAP_CODE_S_TIMER_INTERRUPT 0x80000005
#define TRAP_CODE_M_SOFTWARE_INTERRUPT 0x80000003
#define TRAP_CODE_S_SOFTWARE_INTERRUPT 0x80000001
#define TRAP_CODE_ENVIRONMENT_CALL_U 0x00000008
#define TRAP_CODE_INSTRUCTION_ACCESS_FAULT 0x00000001
#define TRAP_CODE_INSTRUCTION_PAGE_FAULT 0x0000000c
#define TRAP_CODE_LOAD_PAGE_FAULT 0x0000000d
#define TRAP_CODE_STORE_PAGE_FAULT 0x0000000f

#endif
