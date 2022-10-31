#ifndef SIM_H
#define SIM_H

#include "gdbstub_sys.h"
#include "riscv.h"
typedef struct dbg_state sim_t;

// sim_t is declared in gdbstub_sys.h
// simulator general interface
void sim_init(sim_t *);
void sim_single_step(sim_t *);
void sim_resume(sim_t *);
unsigned sim_read_register(sim_t *, unsigned regno);
void sim_write_register(sim_t *, unsigned regno, unsigned value);
unsigned sim_read_csr(sim_t *, unsigned addr);
void sim_write_csr(sim_t *, unsigned addr, unsigned value);
char sim_read_memory(sim_t *, unsigned addr);
void sim_write_memory(sim_t *, unsigned addr, char value);
void sim_fini(sim_t *);
// set callback function on entering debug mode
void sim_debug(sim_t *, void (*func)(sim_t *sim));
// loading elf file to ram
int sim_load_elf(sim_t *, const char *elf_path);
// set block device I/O
int sim_virtio_disk(sim_t *, const char *img_path, int mode);
// set character device I/O
int sim_uart_io(sim_t *, const char *in_path, const char *out_path);

#endif
