#ifndef SIM_H
#define SIM_H

struct memory_t;
struct csr_t;
struct elf_t;

typedef struct sim_t {
  unsigned pc;
  unsigned *gpr;
  struct memory_t *mem;
  struct csr_t *csr;
  struct elf_t *elf;
} sim_t;

void sim_init(sim_t *, const char *elf_path);
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

// trap code below
#define TRAP_CODE_ILLEGAL_INSTRUCTION 0x00000004
#define TRAP_CODE_ENVIRONMENT_CALL_M 0x0000000b
#define TRAP_CODE_ENVIRONMENT_CALL_S 0x00000009
#define TRAP_CODE_BREAKPOINT 0x00000003
#define TRAP_CODE_LOAD_ACCESS_FAULT 0x00000005
#define TRAP_CODE_STORE_ACCESS_FAULT 0x00000007

#endif
