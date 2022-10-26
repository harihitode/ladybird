#ifndef MMIO_H
#define MMIO_H

#include <stdio.h>
#include <threads.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct uart_t {
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
void uart_set_io(uart_t *uart, FILE *in, FILE *out);
char uart_read(uart_t *uart, unsigned addr);
void uart_write(uart_t *uart, unsigned addr, char value);
unsigned uart_irq(const uart_t *uart);
void uart_irq_ack(uart_t *uart);
void uart_fini(uart_t *uart);

typedef struct disk_t {
  struct stat img_stat;
  struct memory_t *mem;
  char *data;
  unsigned queue_num;
  unsigned queue_notify;
  unsigned queue_ppn;
  unsigned page_size;
  unsigned current_queue;
} disk_t;

void disk_init(disk_t *disk);
int disk_load(disk_t *disk, const char *, int rom_mode);
char disk_read(disk_t *disk, unsigned addr);
void disk_write(disk_t *disk, unsigned addr, char value);
unsigned disk_irq(const disk_t *disk);
void disk_irq_ack(disk_t *disk);
void disk_fini(disk_t *disk);

#endif
