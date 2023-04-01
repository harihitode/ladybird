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
struct core_step_result;

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
  // CACHE for RAM
  struct cache_t *dcache;
  struct cache_t *icache;
  struct cache_t *cache;
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

void memory_init(memory_t *, unsigned ram_base, unsigned ram_size, unsigned ram_block_size);
unsigned memory_load(memory_t *, unsigned len, struct core_step_result *result);
unsigned memory_store(memory_t *, unsigned len, struct core_step_result *result);
unsigned memory_load_reserved(memory_t *, unsigned aquire, struct core_step_result *result);
unsigned memory_store_conditional(memory_t *, unsigned aquire, struct core_step_result *result);
unsigned memory_atomic_operation(memory_t *t, unsigned aquire, unsigned release,
                                 unsigned (*op)(unsigned, unsigned),
                                 struct core_step_result *result);
unsigned memory_fence_instruction(memory_t *t);
unsigned memory_fence(memory_t *t, unsigned char predecessor, unsigned char successor);
unsigned memory_fence_tso(memory_t *t);
unsigned memory_dma_send(memory_t *, unsigned pbase, int len, char *data);
unsigned memory_dma_send_c(memory_t *, unsigned pbase, int len, char data);
void memory_fini(memory_t *);
// ram and rom
char *memory_get_page(memory_t *, unsigned addr, unsigned is_write, int device_id);
// [NOTE] memory does not free rom_ptr on fini
void memory_set_rom(memory_t *, const char *, unsigned base, unsigned size, unsigned type);
void memory_set_mmio(memory_t *, struct mmio_t *mmio, unsigned base);
// mmu function
void memory_atp_on(memory_t *, unsigned ppn);
void memory_atp_off(memory_t *);
void memory_tlb_clear(memory_t *);
void memory_icache_invalidate(memory_t *);
void memory_dcache_invalidate(memory_t *);
void memory_dcache_invalidate_line(memory_t *, unsigned paddr);
void memory_dcache_write_back(memory_t *);
unsigned memory_address_translation(memory_t *mem, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv);

void rom_init(rom_t *rom);
void rom_str(rom_t *rom, const char *data, unsigned size);
void rom_mmap(rom_t *rom, const char *img_path, int rom_mode);
void rom_fini(rom_t *rom);

#endif
