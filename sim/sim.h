#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include "gdbstub_sys.h"

// sim_t is declared in gdbstub_sys.h
void sim_init(sim_t *);
void sim_step(sim_t *);
unsigned sim_read_register(sim_t *, unsigned regno);
void sim_write_register(sim_t *, unsigned regno, unsigned value);
char sim_read_memory(sim_t *, unsigned addr);
void sim_write_memory(sim_t *, unsigned addr, char value);
void sim_fini(sim_t *);
// trap
// set callback function when exception occurs
void sim_trap(sim_t *, void (*func)(sim_t *sim));
void sim_clear_exception(sim_t *);
unsigned sim_get_trap_code(sim_t *);
unsigned sim_get_trap_value(sim_t *);
unsigned sim_get_epc(sim_t *);
unsigned sim_get_instruction(sim_t *, unsigned pc);
// mmio
int sim_load_elf(sim_t *, const char *elf_path);
int sim_virtio_disk(sim_t *, const char *img_path, int mode);
int sim_uart_io(sim_t *, FILE *in, FILE *out);
// debug
void sim_debug_enable(sim_t *); // enter debug mode when execute ebreak
void sim_debug_continue(sim_t *);
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

#define INSTRUCTION_EBREAK 0x00100073
#define INSTRUCTION_CEBREAK 0x00009002

#endif
