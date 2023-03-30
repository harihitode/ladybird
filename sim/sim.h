#ifndef SIM_H
#define SIM_H

#include "riscv.h"

// hart
#define HART_NUM 1

// memory map
#define MEMORY_BASE_ADDR_UART   0x10000000
#define MEMORY_BASE_ADDR_DISK   0x10001000
#define MEMORY_BASE_ADDR_ACLINT 0x02000000
#define MEMORY_BASE_ADDR_PLIC   0x0c000000
#define MEMORY_BASE_ADDR_RAM    0x80000000
// 128MiB, 4KiB page RAM
#define RAM_SIZE (128 * 1024 * 1024)
#define RAM_PAGE_SIZE (4 * 1024)
#define RAM_PAGE_OFFS_MASK (RAM_PAGE_SIZE - 1)

// debug address
#define DEVTREE_BLOB_FILE "./ladybird.dtb"
#define DEVTREE_ROM_ADDR 0x00001020
#define DEVTREE_ROM_SIZE 2048
#define CONFIG_ROM_ADDR 0x00001000
#define CONFIG_ROM_SIZE 1024

// advanced core local interrupt map
#define ACLINT_MSIP_BASE (MEMORY_BASE_ADDR_ACLINT + 0x00000000)
#define ACLINT_MTIMECMP_BASE (MEMORY_BASE_ADDR_ACLINT + 0x00004000)
#define ACLINT_SETSSIP_BASE (MEMORY_BASE_ADDR_ACLINT + 0x00008000)
#define ACLINT_MTIME_BASE (MEMORY_BASE_ADDR_ACLINT + 0x0000Bff8)

// IRQ
#define PLIC_MAX_IRQ 10
#define PLIC_MACHINE_CONTEXT 0
#define PLIC_SUPERVISOR_CONTEXT 1
#define PLIC_VIRTIO_MMIO_IRQ_NO 1
#define PLIC_UART_IRQ_NO 10

#define CORE_WINDOW_SIZE 16

#define BUS_ACCESS_READ 0
#define BUS_ACCESS_WRITE 1
#define DEVICE_ID_DMA -1

enum sim_state { running, quit };

struct core_step_result {
  unsigned char prv;
  unsigned long long cycle;
  unsigned pc;
  unsigned pc_paddr;
  unsigned pc_next;
  unsigned inst;
  unsigned opcode;
  unsigned char flush;
  unsigned exception_code;
  unsigned char m_access;
  unsigned m_vaddr;
  unsigned m_paddr;
  unsigned m_data;
  unsigned char trapret;
  unsigned char trigger;

  unsigned char rd_regno;
  unsigned char rd_write_skip;
  unsigned rd_data;
  unsigned char rs1_regno;
  unsigned char rs1_read_skip;
  unsigned char rs2_regno;
  unsigned char rs2_read_skip;

  unsigned inst_window_pos;
  unsigned inst_window_pc[CORE_WINDOW_SIZE];
  unsigned inst_window[CORE_WINDOW_SIZE];

  unsigned char icache_access;
  unsigned char icache_hit;
  unsigned char dcache_access;
  unsigned char dcache_hit;
  unsigned char tlb_access;
  unsigned char tlb_hit;
};

typedef struct sim_t {
  enum sim_state state;
  struct core_t *core;
  struct memory_t *mem;
  struct csr_t *csr;
  struct elf_t *elf;
  struct trigger_t *trigger;
  struct uart_t *uart;
  struct disk_t *disk;
  struct plic_t *plic;
  struct aclint_t *aclint;
  unsigned htif_tohost;
  unsigned htif_fromhost;
  // for debugger
  unsigned dbg_mode;
  char **reginfo;  // register information
  char triple[64]; // triple information
  void (**dbg_handler)(struct sim_t *, unsigned, unsigned, unsigned, unsigned, unsigned);
  void (*stp_handler)(struct core_step_result *, void *arg);
  void *stp_arg;
} sim_t;

// simulator general interface
void sim_init(sim_t *);
void sim_dtb_on(sim_t *);
void sim_config_on(sim_t *);
void sim_single_step(sim_t *);
void sim_resume(sim_t *);
unsigned sim_read_register(sim_t *, unsigned regno);
void sim_write_register(sim_t *, unsigned regno, unsigned value);
unsigned sim_read_csr(sim_t *, unsigned addr);
void sim_write_csr(sim_t *, unsigned addr, unsigned value);
char sim_read_memory(sim_t *, unsigned addr);
void sim_write_memory(sim_t *, unsigned addr, char value);
void sim_cache_flush(sim_t *);
int sim_set_exec_trigger(sim_t *, unsigned addr);
int sim_set_write_trigger(sim_t *, unsigned addr);
int sim_set_read_trigger(sim_t *, unsigned addr);
int sim_set_access_trigger(sim_t *, unsigned addr);
int sim_rst_exec_trigger(sim_t *, unsigned addr);
int sim_rst_write_trigger(sim_t *, unsigned addr);
int sim_rst_read_trigger(sim_t *, unsigned addr);
int sim_rst_access_trigger(sim_t *, unsigned addr);
void sim_fini(sim_t *);
// loading elf file to ram
int sim_load_elf(sim_t *, const char *elf_path);
// set block device I/O
int sim_virtio_disk(sim_t *, const char *img_path, int mode);
// set character device I/O
int sim_uart_io(sim_t *, const char *in_path, const char *out_path);
// debugger helper to tdata
int sim_get_trigger_fired(const sim_t *);
void sim_rst_trigger_hit(sim_t *);
unsigned sim_get_trigger_type(unsigned tdata1);
unsigned sim_match6(unsigned select, unsigned access, unsigned timing);
unsigned sim_icount(unsigned count);

// set callback function on entering debug mode
void sim_set_debug_callback(sim_t *sim, void (*callback)(sim_t *, unsigned, unsigned, unsigned, unsigned, unsigned));
// set callback function on every step
void sim_set_step_callback(sim_t *, void (*func)(struct core_step_result *result, void *));
void sim_set_step_callback_arg(sim_t *sim, void *arg);
void sim_regstat_en(sim_t *sim);

#endif
