#ifndef MMIO_H
#define MMIO_H

#include <stdio.h>

typedef struct uart_t {
  FILE *fi;
  FILE *fo;
  unsigned mode;
} uart_t;

void uart_init(uart_t *uart);
char uart_read(uart_t *uart, unsigned addr);
void uart_write(uart_t *uart, unsigned addr, unsigned value);
void uart_fini(uart_t *uart);

#endif
