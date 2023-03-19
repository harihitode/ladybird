#ifndef MMIO_H
#define MMIO_H

#include <stdio.h>
#ifdef __MACH__
#include <pthread.h>
typedef pthread_t thrd_t;
typedef pthread_mutex_t mtx_t;
#else
#include <threads.h>
#endif

#include "memory.h"

typedef struct uart_t {
  struct mmio_t base;
  int fi;
  int fo;
  unsigned mode;
  char *buf;
  unsigned buf_wr_index;
  unsigned buf_rd_index;
  thrd_t i_thread;
  mtx_t mutex;
  int i_pipe[2];
  unsigned intr_enable;
} uart_t;

void uart_init(uart_t *uart);
void uart_set_io(uart_t *uart, const char *in_path, const char *out_path);
char uart_read(struct mmio_t *uart, unsigned addr);
void uart_write(struct mmio_t *uart, unsigned addr, char value);
unsigned uart_irq(const struct mmio_t *uart);
void uart_irq_ack(struct mmio_t *uart);
void uart_fini(uart_t *uart);

typedef struct disk_t {
  struct mmio_t base;
  struct memory_t *mem;
  struct rom_t *rom;
  unsigned long long host_features;
  unsigned host_features_sel;
  unsigned long long guest_features;
  unsigned guest_features_sel;
  unsigned long long capacity;
  unsigned queue_num;
  unsigned queue_notify;
  unsigned queue_ppn;
  unsigned queue_align;
  unsigned page_size;
  unsigned page_size_mask;
  unsigned current_queue;
  unsigned status;
  unsigned short last_avail_idx;
} disk_t;

void disk_init(disk_t *disk);
int disk_load(disk_t *disk, const char *, int rom_mode);
char disk_read(struct mmio_t *disk, unsigned addr);
void disk_write(struct mmio_t *disk, unsigned addr, char value);
unsigned disk_irq(const struct mmio_t *disk);
void disk_irq_ack(struct mmio_t *disk);
void disk_fini(disk_t *disk);

#endif
