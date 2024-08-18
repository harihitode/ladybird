#ifndef MEMORY_H
#define MEMORY_H

// atomic operations
#define MEMORY_LOAD_DEFAULT 0
#define MEMORY_LOAD_RESERVE 1
#define MEMORY_STORE_DEFAULT 0
#define MEMORY_STORE_CONDITIONAL 1
#define MEMORY_STORE_SUCCESS 0
#define MEMORY_STORE_FAILURE 1

// sram type
#define MEMORY_SRAM_TYPE_DEFAULT 0
#define MEMORY_SRAM_TYPE_MMAP 1
#define MEMORY_SRAM_MODE_READ_WRITE 0
#define MEMORY_SRAM_MODE_READ_ONLY 1

// bus access
#define MEMORY_ACCESS_READ 0
#define MEMORY_ACCESS_WRITE 1
#define MEMORY_ACCESS_DEVICE_ID_DMA -1

#include <sys/mman.h>
#include <sys/stat.h>

struct cache_t;
struct core_step_result;

typedef struct memory_target_t {
  unsigned base;
  unsigned size;
  unsigned reserve_list_len;
  struct { unsigned transaction_id; unsigned begin_addr; unsigned end_addr; } *reserve_list;
  char *(*get_ptr)(struct memory_target_t *target, unsigned addr);
  char (*readb)(struct memory_target_t *unit, unsigned addr);
  void (*writeb)(struct memory_target_t *unit, unsigned addr, char value);
} memory_target_t;

typedef struct mmio_t {
  struct memory_target_t base;
  unsigned (*get_irq)(const struct mmio_t *unit);
  void (*ack_irq)(struct mmio_t *unit);
} mmio_t;

typedef struct sram_t {
  struct memory_target_t base;
  unsigned type;
  char *data;
  struct stat file_stat;
} sram_t;

typedef struct dram_t {
  struct memory_target_t base;
  unsigned block_size;
  unsigned blocks;
  char **block;
} dram_t;

typedef struct memory_t {
  unsigned num_targets;
  struct memory_target_t **targets;
  unsigned num_cache;
  struct cache_t **cache;
} memory_t;

void memory_init(memory_t *);
unsigned memory_load(memory_t *, unsigned len, unsigned reserved, struct core_step_result *result);
unsigned memory_store(memory_t *, unsigned len, unsigned conditional, struct core_step_result *result);
unsigned memory_cpy_to(memory_t *, int device_id, unsigned dst, const char *data, int len);
unsigned memory_cpy_from(memory_t *, int device_id, char *dst, unsigned src, int len);
unsigned memory_set(memory_t *, int device_id, unsigned dst, char c, int len);
void memory_add_target(memory_t *, memory_target_t *, unsigned base, unsigned size);
void memory_add_cache(memory_t *, struct cache_t *);
void memory_cache_coherent(memory_t *, unsigned addr, unsigned len, int is_write, int device_id);
void memory_fini(memory_t *);

void memory_target_init(memory_target_t *target, unsigned base, unsigned size,
                        char *(*get_ptr)(struct memory_target_t *target, unsigned addr),
                        char (*readb)(struct memory_target_t *target, unsigned addr),
                        void (*writeb)(struct memory_target_t *target, unsigned addr, char value));
char memory_target_readb(struct memory_target_t *unit, unsigned transaction_id, unsigned addr);
void memory_target_writeb(struct memory_target_t *unit, unsigned transaction_id, unsigned addr, char value);
void memory_target_set_reserve_flag(struct memory_target_t *unit, unsigned transaction_id, unsigned addr, unsigned len);
int memory_target_get_reserve_flag(struct memory_target_t *unit, unsigned transaction_id, unsigned addr, unsigned len);
char *memory_target_get_ptr(struct memory_target_t *unit, unsigned addr);
void memory_target_fini(memory_target_t *target);

void sram_init_with_char(sram_t *sram, const char data, unsigned size);
void sram_init_with_str(sram_t *sram, const char *data, unsigned size);
void sram_init_with_file(sram_t *sram, const char *img_path, int mode);
char sram_readb(struct memory_target_t *sram, unsigned addr);
void sram_writeb(struct memory_target_t *sram, unsigned addr, char value);
void sram_fini(sram_t *sram);

void dram_init(dram_t *dram, unsigned size, unsigned block_size);
char *dram_get_ptr(struct memory_target_t *dram, unsigned addr);
char dram_readb(struct memory_target_t *dram, unsigned addr);
void dram_writeb(struct memory_target_t *dram, unsigned addr, char value);
void dram_fini(dram_t *dram);

#endif
