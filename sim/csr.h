#ifndef CSR_H
#define CSR_H

#include <stdint.h>

struct sim_t;

typedef struct csr_t {
  struct sim_t *sim;
  void (*trap_handler)(struct sim_t *);
  uint64_t cycle;
  uint64_t time;
  uint64_t timecmp;
  uint64_t instret;;
  unsigned mode;
  unsigned hartid;
  unsigned mepc;
  unsigned mcause;
  unsigned mscratch;
  unsigned mtval; // [M] trap value
  unsigned mtvec; // [M] trap base address
  unsigned mie; // [M] interrupt enable
  unsigned stvec; // [S] trap base address
  unsigned stval; // [S] trap value
  unsigned sie; // [S] interrupt enable
} csr_t;

void csr_init(csr_t *);
void csr_set_sim(csr_t *, struct sim_t *);
unsigned csr_csrr(csr_t *, unsigned addr);
void csr_csrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrs(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrc(csr_t *, unsigned addr, unsigned value);
// trap
void csr_trap(csr_t *, unsigned trap_code);
void csr_set_tval(csr_t *, unsigned trap_value);
unsigned csr_get_tval(csr_t *);
// timer
uint64_t csr_get_timecmp(csr_t *);
void csr_set_timecmp(csr_t *, uint64_t);
int csr_get_timerint(csr_t *);
void csr_fini(csr_t *);

// mode
#define PRIVILEGE_MODE_U 0x0
#define PRIVILEGE_MODE_S 0x1
#define PRIVILEGE_MODE_M 0x3
// csr address
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

#endif
