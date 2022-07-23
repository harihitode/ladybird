#include "mmio.h"

const unsigned UART_MODE_DEFAULT = 3;
const unsigned UART_MODE_SETBAUD_RATE = (1 << 7);

void uart_init(uart_t *uart) {
  uart->fi = stdin;
  uart->fo = stdout;
  uart->mode = UART_MODE_DEFAULT;
}

char uart_read(uart_t *uart, unsigned addr) {
  switch (addr) {
  case 0x10000000: // RX Register
    return 'a';
  case 0x10000005: // Line Status Register
    return 0x21; // always ready
  default:
    return 0;
  }
}

void uart_write(uart_t *uart, unsigned addr, unsigned value) {
  switch (addr) {
  case 0x10000000:
    if (uart->mode == UART_MODE_DEFAULT) {
      fputc(value, uart->fo);
    }
    break;
  case 0x10000003: // Line Control Register
    uart->mode = value;
    break;
  default:
    break;
  }
  return;
}

void uart_fini(uart_t *uart) {
  fclose(uart->fi);
  fclose(uart->fo);
  return;
}
