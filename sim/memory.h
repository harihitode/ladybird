#ifndef MEMORY_H
#define MEMORY_H

// memory layout
#define MEMORY_BASE_ADDR_UART 0x10000000
#define MEMORY_BASE_ADDR_DISK 0x10001000
#define MEMORY_BASE_ADDR_RAM  0x80000000

struct uart_t;
struct disk_t;

typedef struct memory_t {
  unsigned blocks;
  unsigned *base;
  char **block;
  char *reserve;
  // MMU
  char vmflag;
  unsigned vmbase;
  // MMIO
  struct uart_t *uart;
  struct disk_t *disk;
} memory_t;

void memory_init(memory_t *);
char memory_load(memory_t *, unsigned addr);
void memory_store(memory_t *,unsigned addr, char value);
void memory_fini(memory_t *);
// atomic
unsigned memory_load_reserved(memory_t *, unsigned addr);
unsigned memory_store_conditional(memory_t *, unsigned addr, unsigned value);
// mmu
void memory_atp_on(memory_t *, unsigned ppn);
void memory_atp_off(memory_t *);

#endif
