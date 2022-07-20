#ifndef CSR_H
#define CSR_H

struct sim_t;

typedef struct csr_t {
  struct sim_t *sim;
  void (*trap_handler)(unsigned, struct sim_t *);
  unsigned mepc;
  unsigned hartid;
  unsigned atp; // address translation & protection
  unsigned sie; // [S] interrupt enable
  unsigned mscratch;
  unsigned mtvec; // trap base address
  unsigned mie; // [M] interrupt enable
} csr_t;

void csr_init(csr_t *);
unsigned csr_csrr(csr_t *, unsigned addr);
void csr_csrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrw(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrs(csr_t *, unsigned addr, unsigned value);
unsigned csr_csrrc(csr_t *, unsigned addr, unsigned value);
void csr_trap(csr_t *, unsigned trap_code);
void csr_fini(csr_t *);

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

#endif
