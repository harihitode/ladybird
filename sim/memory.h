#ifndef MEMORY_H
#define MEMORY_H

// memory layout
#define MEMORY_STORE_SUCCESS 0
#define MEMORY_STORE_FAILURE 1

// rom type
#define MEMORY_ROM_TYPE_DEFAULT 0
#define MEMORY_ROM_TYPE_MMAP 1

#include <sys/mman.h>
#include <sys/stat.h>

struct mmio_t;
struct cache_t;
struct tlb_t;

struct mmio_t {
  unsigned base;
  unsigned size;
  char (*readb)(struct mmio_t* unit, unsigned addr);
  void (*writeb)(struct mmio_t* unit, unsigned addr, char value);
  unsigned (*get_irq)(const struct mmio_t *unit);
  void (*ack_irq)(struct mmio_t *unit);
};

typedef struct rom_t {
  unsigned base;
  unsigned size;
  char *data;
  unsigned rom_type;
  struct stat file_stat;
  struct rom_t *next;
} rom_t;

typedef struct memory_t {
  // RAM
  unsigned ram_base;
  unsigned ram_size;
  unsigned ram_block_size;
  unsigned ram_blocks;
  char **ram_block;
  char *ram_reserve;
  // CACHE for RAM
  struct cache_t *dcache;
  struct cache_t *icache;
  // MMU
  // To support a physical address space larger than 4GiB,
  // RV32 stores a PPN in satp, ranther than a physical address.
  unsigned vmrppn; // root physical page number
  char vmflag; // true: virtual memory on
  struct tlb_t *tlb;
  // MMIO
  struct mmio_t **mmio_list;
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
  unsigned megapage;
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

void memory_init(memory_t *, unsigned ram_base, unsigned ram_size, unsigned ram_block_size);
unsigned memory_load(memory_t *, unsigned addr, unsigned *value, unsigned size, unsigned prv);
unsigned memory_load_instruction(memory_t *, unsigned addr, unsigned *value, unsigned prv);
unsigned memory_store(memory_t *,unsigned addr, unsigned value, unsigned size, unsigned prv);
unsigned memory_dma_send(memory_t *, unsigned pbase, int len, char *data);
unsigned memory_dma_send_c(memory_t *, unsigned pbase, int len, char data);
void memory_fini(memory_t *);
// ram and rom
char *memory_get_page(memory_t *, unsigned addr, unsigned is_write, int device_id);
// [NOTE] memory does not free rom_ptr on fini
void memory_set_rom(memory_t *, const char *, unsigned base, unsigned size, unsigned type);
void memory_set_mmio(memory_t *, struct mmio_t *mmio, unsigned base);
// atomic
unsigned memory_load_reserved(memory_t *, unsigned addr, unsigned *value, unsigned prv);
unsigned memory_store_conditional(memory_t *, unsigned addr, unsigned value, unsigned *success, unsigned prv);
// mmu function
void memory_atp_on(memory_t *, unsigned ppn);
void memory_atp_off(memory_t *);
void memory_tlb_clear(memory_t *);
void memory_icache_invalidate(memory_t *);
void memory_dcache_invalidate(memory_t *);
void memory_dcache_invalidate_line(memory_t *, unsigned paddr);
void memory_dcache_write_back(memory_t *);
unsigned memory_address_translation(memory_t *mem, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv);

#define CACHE_WRITE 1
#define CACHE_READ 0

// cache (instruction or data)
void cache_init(cache_t *, memory_t *, unsigned line_len, unsigned line_size);
char *cache_get(cache_t *, unsigned addr, char write);
int cache_write_back(cache_t *, unsigned line);
void cache_fini(cache_t *);

// translate lookaside buffer
void tlb_init(tlb_t *, memory_t *, unsigned line_size);
unsigned tlb_get(tlb_t *, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv);
void tlb_clear(tlb_t *);
void tlb_fini(tlb_t *);

void rom_init(rom_t *rom);
void rom_str(rom_t *rom, const char *data, unsigned size);
void rom_mmap(rom_t *rom, const char *img_path, int rom_mode);
void rom_fini(rom_t *rom);

#endif
