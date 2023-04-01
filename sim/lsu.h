#ifndef LSU_H
#define LSU_H

#define RW_CACHE 1
#define RO_CACHE 0

struct memory_t;
struct core_step_result;

#define CACHE_INVALID 0
#define CACHE_SHARED 1
#define CACHE_MODIFIED 2

#define CACHE_ACCESS_READ 0
#define CACHE_ACCESS_WRITE 1

typedef struct cache_line_t {
  unsigned char state;
  unsigned char reserved;
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
  int hart_id;
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
  int hart_id;
} tlb_t;

typedef struct lsu_t {
  struct memory_t *mem;
  // MMU
  // To support a physical address space larger than 4GiB,
  // RV32 stores a PPN in satp, ranther than a physical address.
  unsigned vmrppn; // root physical page number
  char vmflag; // true: virtual memory on
  struct tlb_t *tlb;
  // CACHE for RAM
  struct cache_t *dcache;
  struct cache_t *icache;
} lsu_t;

void lsu_init(lsu_t *, struct memory_t *mem);
// Core functions
unsigned lsu_load(lsu_t *t, unsigned len, struct core_step_result *result);
unsigned lsu_store(lsu_t *t, unsigned len, struct core_step_result *result);
unsigned lsu_load_reserved(lsu_t *t, unsigned aquire, struct core_step_result *result);
unsigned lsu_store_conditional(lsu_t *t, unsigned release, struct core_step_result *result);
unsigned lsu_atomic_operation(lsu_t *t, unsigned aquire, unsigned release,
                              unsigned (*op)(unsigned, unsigned),
                              struct core_step_result *result);
unsigned lsu_fence_instruction(lsu_t *t);
unsigned lsu_fence(lsu_t *t, unsigned char predecessor, unsigned char successor);
unsigned lsu_fence_tso(lsu_t *t);
// MMU functions
void lsu_atp_on(lsu_t *, unsigned ppn);
unsigned lsu_atp_get(lsu_t *);
void lsu_atp_off(lsu_t *);
void lsu_tlb_clear(lsu_t *);
void lsu_icache_invalidate(lsu_t *);
void lsu_dcache_invalidate(lsu_t *);
void lsu_dcache_invalidate_line(lsu_t *, unsigned paddr);
void lsu_dcache_write_back(lsu_t *);
unsigned lsu_address_translation(lsu_t *mem, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv);
void lsu_fini(lsu_t *);

// cache (instruction or data)
void cache_init(cache_t *, struct memory_t *, unsigned line_len, unsigned line_size);
cache_line_t *cache_get_line(cache_t *, unsigned addr, int is_write);
char *cache_get_line_ptr(cache_t *, unsigned addr, int is_write);
int cache_write_back(cache_t *, unsigned line);
void cache_fini(cache_t *);

// translate lookaside buffer
void tlb_init(tlb_t *, struct memory_t *, unsigned line_size);
unsigned tlb_get(tlb_t *, unsigned basepte, unsigned vaddr, unsigned *paddr, unsigned access_type, unsigned prv);
void tlb_clear(tlb_t *);
void tlb_fini(tlb_t *);

#endif
