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
// set callback function when trap occurs, the callback receives trap code
void sim_trap(sim_t *, void (*func)(unsigned trap_code, sim_t *sim));
unsigned sim_read_register(sim_t *, unsigned regno);
void sim_write_register(sim_t *, unsigned regno, unsigned value);
unsigned sim_read_memory(sim_t *, unsigned addr);
void sim_write_memory(sim_t *, unsigned addr, unsigned value);
void sim_fini(sim_t *);

// trap code below
#define TRAP_CODE_INVALID_INSTRUCTION 0
#define TRAP_CODE_ECALL 1
#define TRAP_CODE_EBREAK 2

#endif
