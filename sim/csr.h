#ifndef CSR_H
#define CSR_H

struct sim_t;

typedef struct csr_t {
  struct sim_t *sim;
  void (*trap_handler)(unsigned, struct sim_t *);
  unsigned mepc;
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

#endif
