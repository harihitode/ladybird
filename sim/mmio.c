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
  case 0x00000000: // RX Register
    return 'a';
  case 0x00000005: // Line Status Register
    return 0x21; // always ready
  default:
    return 0;
  }
}

void uart_write(uart_t *uart, unsigned addr, char value) {
  switch (addr) {
  case 0x00000000:
    if (uart->mode == UART_MODE_DEFAULT) {
      fputc(value, uart->fo);
    }
    break;
  case 0x00000003: // Line Control Register
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

void disk_init(disk_t *disk) {
  disk->data = NULL;
}

int disk_load(disk_t *disk, const char *img_path) {
  FILE *fp = NULL;
  if ((fp = fopen(img_path, "r")) == NULL) {
    perror("disk file open");
    return 1;
  }
  fstat(fileno(fp), &disk->img_stat);
  disk->data = (char *)mmap(NULL, disk->img_stat.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
  if (disk->data == MAP_FAILED) {
    perror("disk mmap");
    fclose(fp);
    disk->data = NULL;
    return 1;
  }
  return 0;
}

#define VIRTIO_MMIO_MAGIC_VALUE_0 0x000
#define VIRTIO_MMIO_MAGIC_VALUE_1 0x001
#define VIRTIO_MMIO_MAGIC_VALUE_2 0x002
#define VIRTIO_MMIO_MAGIC_VALUE_3 0x003
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID_0 0x00c
#define VIRTIO_MMIO_VENDOR_ID_1 0x00d
#define VIRTIO_MMIO_VENDOR_ID_2 0x00e
#define VIRTIO_MMIO_VENDOR_ID_3 0x00f
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038

char disk_read(disk_t *disk, unsigned addr) {
  char ret = 0;
  switch (addr) {
  case VIRTIO_MMIO_MAGIC_VALUE_0:
    ret = 0x76; // magic
    break;
  case VIRTIO_MMIO_MAGIC_VALUE_1:
    ret = 0x69; // magic
    break;
  case VIRTIO_MMIO_MAGIC_VALUE_2:
    ret = 0x72; // magic
    break;
  case VIRTIO_MMIO_MAGIC_VALUE_3:
    ret = 0x74; // magic
    break;
  case VIRTIO_MMIO_VERSION:
    ret = 1; // legacy
    break;
  case VIRTIO_MMIO_DEVICE_ID:
    ret = 2; // disk
    break;
  case VIRTIO_MMIO_VENDOR_ID_0:
    ret = 0x51;
    break;
  case VIRTIO_MMIO_VENDOR_ID_1:
    ret = 0x45;
    break;
  case VIRTIO_MMIO_VENDOR_ID_2:
    ret = 0x4d;
    break;
  case VIRTIO_MMIO_VENDOR_ID_3:
    ret = 0x55;
    break;
  case VIRTIO_MMIO_QUEUE_NUM_MAX:
    ret = 8;
    break;
  case VIRTIO_MMIO_QUEUE_NUM:
    ret = 0;
    break;
  default:
    ret = 0;
    break;
  }
  return ret;
}

void disk_write(disk_t *disk, unsigned addr, char value) {
  return;
}

void disk_fini(disk_t *disk) {
  if (disk->data) {
    munmap(disk->data, disk->img_stat.st_size);
  }
  return;
}
