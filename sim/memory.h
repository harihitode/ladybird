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

struct uart_t;
struct disk_t;
struct csr_t;
struct cache_t;
struct tlb_t;

struct rom_t {
  unsigned base;
  unsigned size;
  char *rom;
  struct rom_t *next;
};

typedef struct memory_t {
  unsigned ram_size;
  unsigned ram_block_size;
  unsigned ram_blocks;
  char **ram_block;
  char *ram_reserve;
  // cache for ram
  struct cache_t *dcache;
  struct cache_t *icache;
  struct tlb_t *tlb;
  struct csr_t *csr;
  // cache for instruction
  char *inst_line;
  unsigned inst_line_pc;
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
  // ROM
  struct rom_t *rom_list;
} memory_t;

typedef struct cache_line_t {
  unsigned valid;
  unsigned dirty;
  unsigned tag;
  char *data;
} cache_line_t;

typedef struct cache_t {
  struct memory_t *mem;
  unsigned line_len; // should be power of 2
  unsigned line_size; // should be power of 2
  cache_line_t *line;
  // mask
  unsigned index_mask;
  unsigned tag_mask;
  unsigned line_mask;
  // performance counter
  unsigned long access_count;
  unsigned long hit_count;
} cache_t;

typedef struct tlb_line_t {
  unsigned valid;
  unsigned dirty;
  unsigned tag;
  unsigned value;
} tlb_line_t;

typedef struct tlb_t {
  struct memory_t *mem;
  unsigned line_size; // should be power of 2
  tlb_line_t *line;
  // mask
  unsigned index_mask;
  unsigned tag_mask;
  // performance counter
  unsigned long access_count;
  unsigned long hit_count;
} tlb_t;

void memory_init(memory_t *, unsigned ram_size, unsigned ram_block_size);
unsigned memory_load(memory_t *, unsigned addr, unsigned size, unsigned reserved);
unsigned memory_load_instruction(memory_t *, unsigned addr);
unsigned memory_store(memory_t *,unsigned addr, unsigned value, unsigned size, unsigned conditional);
void memory_fini(memory_t *);
// ram and rom
char *memory_get_page(memory_t *, unsigned);
// [NOTE] memory does not free rom_ptr on fini
void memory_set_rom(memory_t *, unsigned base, unsigned size, char *rom_ptr);
// atomic
unsigned memory_load_reserved(memory_t *, unsigned addr);
unsigned memory_store_conditional(memory_t *, unsigned addr, unsigned value);
// mmu function
void memory_atp_on(memory_t *, unsigned ppn);
void memory_atp_off(memory_t *);
void memory_tlb_clear(memory_t *);
void memory_icache_invalidate(memory_t *);
void memory_dcache_write_back(memory_t *);
unsigned memory_address_translation(memory_t *mem, unsigned addr, unsigned access_type);

// cache (instruction or data)
void cache_init(cache_t *, memory_t *, unsigned line_len, unsigned line_size);
char *cache_get(cache_t *, unsigned addr, char write);
int cache_write_back(cache_t *, unsigned line);
void cache_fini(cache_t *);

// translate lookaside buffer
void tlb_init(tlb_t *, memory_t *, unsigned line_size);
unsigned tlb_get(tlb_t *, unsigned addr, unsigned access_type);
void tlb_clear(tlb_t *);
void tlb_fini(tlb_t *);

#endif
