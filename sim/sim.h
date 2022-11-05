#ifndef SIM_H
#define SIM_H

#include "gdbstub_sys.h"
#include "riscv.h"
typedef struct dbg_state sim_t;

// memory map
#define MEMORY_BASE_ADDR_UART   0x10000000
#define MEMORY_BASE_ADDR_DISK   0x10001000
#define MEMORY_BASE_ADDR_ACLINT 0x02000000
#define MEMORY_BASE_ADDR_PLIC   0x0c000000
#define MEMORY_BASE_ADDR_RAM    0x80000000
// 128GiB, 4KiB page RAM
#define RAM_SIZE (128 * 1024 * 1024)
#define RAM_PAGE_SIZE (4 * 1024)

// advanced core local interrupt map
#define ACLINT_MTIMER_MTIMECMP_BASE (MEMORY_BASE_ADDR_ACLINT)
#define ACLINT_MTIMER_MTIME_BASE (MEMORY_BASE_ADDR_ACLINT + 0x00007ff8)
#define ACLINT_MSWI_BASE (MEMORY_BASE_ADDR_ACLINT + 0x00008000)
#define ACLINT_SSWI_BASE (MEMORY_BASE_ADDR_ACLINT + 0x0000c000)

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
// cache_flush
void sim_cache_flush(sim_t *);
// debugger helper to tdata
unsigned sim_match6(unsigned select, unsigned access, unsigned timing);
unsigned sim_icount(unsigned count);

#endif
