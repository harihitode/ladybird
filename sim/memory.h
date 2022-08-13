#ifndef MEMORY_H
#define MEMORY_H

// memory layout
#define MEMORY_BASE_ADDR_UART   0x10000000
#define MEMORY_BASE_ADDR_DISK   0x10001000
#define MEMORY_BASE_ADDR_ACLINT 0x02000000
#define MEMORY_BASE_ADDR_PLIC   0x0c000000
#define MEMORY_BASE_ADDR_RAM    0x80000000

#define MEMORY_STORE_SUCCESS 0
#define MEMORY_STORE_FAILURE 1

struct sim_t;
struct uart_t;
struct disk_t;
struct csr_t;

typedef struct memory_t {
  unsigned blocks;
  unsigned *base;
  char **block;
  char *reserve;
  struct csr_t *csr;
  // MMU
  char vmflag;
  // To support a physical address space larger than 4GiB,
  // RV32 stores a PPN in satp, ranther than a physical address.
  unsigned vmrppn; // root physical page number
  // MMIO
  struct uart_t *uart;
  struct disk_t *disk;
  // PLIC
  struct plic_t *plic;
} memory_t;

void memory_init(memory_t *);
void memory_set_sim(memory_t *, struct sim_t *);
unsigned memory_load(memory_t *, unsigned addr, unsigned size, unsigned reserved);
unsigned memory_store(memory_t *,unsigned addr, unsigned value, unsigned size, unsigned conditional);
void memory_fini(memory_t *);
char *memory_get_page(memory_t *, unsigned);
// atomic
unsigned memory_load_reserved(memory_t *, unsigned addr);
unsigned memory_store_conditional(memory_t *, unsigned addr, unsigned value);
// mmu
void memory_atp_on(memory_t *, unsigned ppn);
void memory_atp_off(memory_t *);

#endif
