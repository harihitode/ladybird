#ifndef SIM_H
#define SIM_H

#include "gdbstub_sys.h"

// sim_t is declared in gdbstub_sys.h
// simulator general interface
void sim_init(sim_t *);
void sim_step(sim_t *);
unsigned sim_get_mode(sim_t *);
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

// mode
#define PRIVILEGE_MODE_U 0x0
#define PRIVILEGE_MODE_S 0x1
#define PRIVILEGE_MODE_M 0x3
#define PRIVILEGE_MODE_D 0x4

// csr addr
#define CSR_ADDR_M_EPC 0x00000341
#define CSR_ADDR_M_HARTID 0x00000f14
#define CSR_ADDR_M_STATUS 0x00000300
#define CSR_ADDR_S_ATP 0x00000180
#define CSR_ADDR_M_EDELEG 0x00000302
#define CSR_ADDR_M_IDELEG 0x00000303
#define CSR_ADDR_S_IE 0x00000104
#define CSR_ADDR_M_PMPADDR0 0x000003b0
#define CSR_ADDR_M_PMPCFG0 0x000003a0
#define CSR_ADDR_M_SCRATCH 0x00000340
#define CSR_ADDR_M_TVEC 0x00000305
#define CSR_ADDR_M_IE 0x00000304
#define CSR_ADDR_S_STATUS 0x00000100
#define CSR_ADDR_S_TVEC 0x00000105
#define CSR_ADDR_U_CYCLE 0x00000c00
#define CSR_ADDR_U_TIME 0x00000c01
#define CSR_ADDR_U_INSTRET 0x00000c02
#define CSR_ADDR_U_CYCLEH 0x00000c80
#define CSR_ADDR_U_TIMEH 0x00000c81
#define CSR_ADDR_U_INSTRETH 0x00000c82
#define CSR_ADDR_M_IP 0x00000344
#define CSR_ADDR_S_IP 0x00000144
#define CSR_ADDR_S_EPC 0x00000141
#define CSR_ADDR_S_CAUSE 0x00000142
#define CSR_ADDR_S_TVAL 0x00000143
#define CSR_ADDR_S_SCRATCH 0x00000140
#define CSR_ADDR_M_CAUSE 0x00000342
#define CSR_ADDR_M_TVAL 0x00000343
#define CSR_ADDR_D_CSR 0x000007b0
#define CSR_ADDR_D_PC 0x000007b1

// debug cause
#define CSR_DCSR_ENABLE_ANY_BREAK 0x0003b003
#define CSR_DCSR_CAUSE_EBREAK 0x1

#endif
