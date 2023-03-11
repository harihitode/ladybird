#include "riscv.h"
#include "mmio.h"
#include "memory.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// ns16650a (see `http://byterunner.com/16550.html`)
#define UART_ADDR_RHR 0
#define UART_ADDR_THR 0
#define UART_ADDR_IER 1
#define UART_IER_ENABLE 0x03
#define UART_ADDR_FCR 2
#define UART_FCR_CLEAR (3 << 1)
#define UART_ADDR_ISR 2
#define UART_ADDR_LCR 3
#define UART_LCR_DEFAULT_MODE 3
#define UART_LCR_SETBAUD_RATE_MODE (1 << 7)
#define UART_ADDR_MCR 4
#define UART_ADDR_LSR 5
#define UART_ADDR_MSR 6
#define UART_LSR_RX_READY (1 << 0)
#define UART_LSR_TX_IDLE (1 << 5)

#define UART_BUF_SIZE 512

#ifdef __MACH__
enum {
  thrd_error = 0,
  thrd_success = 1
};
enum {
  mtx_plain = 0,
  mtx_recursive = 1,
  mtx_timed = 2
};
typedef void *(*thrd_start_t)(void *);
inline int thrd_create(thrd_t *thr, thrd_start_t start_routine, void *arg) {
  if (pthread_create(thr, NULL, start_routine, arg) == 0) {
    return thrd_success;
  } else {
    return thrd_error;
  }
}
_Noreturn void thrd_exit(int res) { pthread_exit((void *)(intptr_t)res); }
inline int thrd_join(thrd_t thr, int *res) {
  void *pres;
  if (pthread_join(thr, &pres) != 0) {
    return thrd_error;
  }
  if (res != NULL) {
    *res = (int)(intptr_t)pres;
  }
  return thrd_success;
}
inline int mtx_init(mtx_t *mtx, int type) {
  if ((type == mtx_plain) && (pthread_mutex_init(mtx, NULL) == 0)) {
    return thrd_success;
  } else {
    return thrd_error;
  }
}
inline int mtx_lock(mtx_t *mtx) {
  if (pthread_mutex_lock(mtx) == 0) {
    return thrd_success;
  } else {
    return thrd_error;
  }
}
inline int mtx_unlock(mtx_t *mtx) {
  if (pthread_mutex_unlock(mtx) == 0) {
    return thrd_success;
  } else {
    return thrd_error;
  }
}
inline int mtx_destroy(mtx_t *mtx) {
  if (pthread_mutex_destroy(mtx) == 0) {
    return thrd_success;
  } else {
    return thrd_error;
  }
}
#endif

static int uart_input_routine(void *arg) {
  uart_t *uart = (uart_t *)arg;
  char c;
  int ret = 0;
  int loop = 1;
  if (!isatty(uart->fi)) {
    while (loop) {
      if ((ret = read(uart->fi, &c, 1)) < 0) {
        perror("uart routine read");
        loop = 0;
      } else {
        if (ret == 0) {
          loop = 0;
        } else {
          mtx_lock(&uart->mutex);
          uart->buf[uart->buf_wr_index++] = c;
          mtx_unlock(&uart->mutex);
        }
      }
    }
  } else {
    // prepare for select
    int select_maxfd = (uart->fi > uart->i_pipe[0]) ? uart->fi : uart->i_pipe[0];
    while (loop) {
      fd_set select_fds;
      FD_ZERO(&select_fds);
      FD_SET(uart->fi, &select_fds);
      FD_SET(uart->i_pipe[0], &select_fds);
      select(select_maxfd + 1, &select_fds, NULL, NULL, NULL);

      if (FD_ISSET(uart->i_pipe[0], &select_fds)) {
        loop = 0;
      }
      if (FD_ISSET(uart->fi, &select_fds)) {
        if ((ret = read(uart->fi, &c, 1)) < 0) {
          perror("uart routine read");
          loop = 0;
        } else {
          if (ret == 0) {
            loop = 0;
          } else {
            mtx_lock(&uart->mutex);
            uart->buf[uart->buf_wr_index++] = c;
            mtx_unlock(&uart->mutex);
          }
        }
      }
    }
  }
  thrd_exit(ret);
}

void uart_init(uart_t *uart) {
  uart->base.base = 0;
  uart->base.size = 4096;
  uart->base.readb = uart_read;
  uart->base.writeb = uart_write;
  uart->mode = UART_LCR_DEFAULT_MODE;
  uart->buf = (char *)malloc(UART_BUF_SIZE * sizeof(char));
  uart->intr_enable = 0;
  if (pipe(uart->i_pipe) == -1) {
    perror("uart init pipe for child thread");
  }
  mtx_init(&uart->mutex, mtx_plain);
  uart->fi = -1;
  uart->fo = -1;
  uart_set_io(uart, NULL, NULL);
}

static void uart_unset_io(uart_t *uart) {
  if (uart->fi >= 0) {
    char c = 'a';
    if (uart->fo >= 3) {
      close(uart->fo);
      uart->fo = STDOUT_FILENO;
    }
    if (isatty(uart->fi)) {
      if (write(uart->i_pipe[1], &c, 1) < 0) {
        perror("uart fini write notification");
      }
    }
    if (uart->fi >= 3) {
      close(uart->fi);
      uart->fi = STDIN_FILENO;
    }
    uart->buf_wr_index = 0;
    uart->buf_rd_index = 0;
    thrd_join(uart->i_thread, NULL);
  }
}

void uart_set_io(uart_t *uart, const char *in_path, const char *out_path) {
  uart_unset_io(uart);
  if (in_path == NULL) {
    uart->fi = STDIN_FILENO;
  } else {
    if ((uart->fi = open(in_path, O_RDONLY)) < 0) {
      perror("uart input open");
    }
  }
  if (out_path == NULL) {
    uart->fo = STDOUT_FILENO;
  } else {
    if ((uart->fo = open(out_path, O_WRONLY)) < 0) {
      perror("uart output open");
    }
  }
  if (thrd_create(&uart->i_thread, (thrd_start_t)uart_input_routine, (void *)uart) == thrd_error) {
    fprintf(stderr, "uart initialization error: thread create\n");
  }
  return;
}

char uart_read(struct mmio_t *unit, unsigned addr) {
  addr -= unit->base;
  uart_t *uart = (uart_t *)unit;
  switch (addr) {
  case UART_ADDR_RHR: {
    // RX Register (uart input)
    char ret = 0;
    mtx_lock(&uart->mutex);
    ret = uart->buf[uart->buf_rd_index++];
    if (uart->buf_rd_index == uart->buf_wr_index) {
      uart->buf_rd_index = 0;
      uart->buf_wr_index = 0;
    }
    mtx_unlock(&uart->mutex);
    return ret;
  }
  case UART_ADDR_ISR:
    return (
            1 << 7 |
            1 << 6 |
            1
            );
  case UART_ADDR_IER:
    return uart->intr_enable;
  case UART_ADDR_MCR: // Modem Control Register
    return 0;
  case UART_ADDR_MSR:

  case UART_ADDR_LSR: // Line Status Register
    {
      char ret = UART_LSR_TX_IDLE; // as default tx idle
      if (uart->buf_wr_index > uart->buf_rd_index) {
        ret |= UART_LSR_RX_READY;
      }
      return ret;
    }
  default:
#if 0
    fprintf(stderr, "uart unknown address read: %08x\n", addr);
#endif
    return 0;
  }
}

void uart_write(struct mmio_t *unit, unsigned addr, char value) {
  addr -= unit->base;
  uart_t *uart = (uart_t *)unit;
  switch (addr) {
  case UART_ADDR_IER:
    uart->intr_enable = value;
    break;
  case UART_ADDR_FCR:
    if (value & UART_FCR_CLEAR) {
      mtx_lock(&uart->mutex);
      uart->buf_wr_index = 0;
      uart->buf_rd_index = 0;
      mtx_unlock(&uart->mutex);
    }
    break;
  case UART_ADDR_THR:
    // TX Register (uart output)
    if (!(uart->mode & UART_LCR_SETBAUD_RATE_MODE)) {
      if (write(uart->fo, &value, 1) < 0) {
        perror("uart output");
      }
    }
    break;
  case UART_ADDR_MCR:
    // not implemented
    break;
  case UART_ADDR_LCR: // Line Control Register
    uart->mode = value;
    break;
  default:
#if 0
    fprintf(stderr, "uart unknown address write: %08x <- %08x\n", addr, value);
#endif
    break;
  }
  return;
}

unsigned uart_irq(const uart_t *uart) {
  if (uart->intr_enable && (uart->buf_wr_index > uart->buf_rd_index)) {
    return 1;
  } else {
    return 0;
  }
}

void uart_irq_ack(uart_t *uart) {
  return;
}

void uart_fini(uart_t *uart) {
  uart_unset_io(uart);
  mtx_destroy(&uart->mutex);
  if (uart->buf) {
    free(uart->buf);
  }
  close(uart->i_pipe[0]);
  close(uart->i_pipe[1]);
  return;
}

void disk_init(disk_t *disk) {
  disk->base.base = 0;
  disk->base.size = 4096;
  disk->base.readb = disk_read;
  disk->base.writeb = disk_write;
  disk->mem = NULL;
  disk->rom = (rom_t *)calloc(1, sizeof(rom_t));
  rom_init(disk->rom);
  disk->current_queue = 0;
  disk->queue_num = 0;
  disk->queue_notify = 0;
  disk->queue_ppn = 0;
  disk->page_size = 0;
  disk->page_size_mask = 0;
}

int disk_load(disk_t *disk, const char *img_path, int rom_mode) {
  rom_mmap(disk->rom, img_path, rom_mode);
  return 0;
}

#define VIRTIO_MMIO_MAGIC_VALUE 0x000
#define VIRTIO_MMIO_VERSION 0x004
#define VIRTIO_MMIO_DEVICE_ID 0x008
#define VIRTIO_MMIO_VENDOR_ID 0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010 // [TODO] feature negotiation
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020 // [TODO] feature negatiation
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_SELECT_QUEUE 0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
#define VIRTIO_MMIO_QUEUE_NUM 0x038
#define VIRTIO_MMIO_QUEUE_PFN 0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060 // read only
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064 // write only
#define VIRTIO_MMIO_STATUS 0x070

const unsigned virtio_mmio_magic = 0x74726976;
const unsigned virtio_mmio_vendor_id = 0x554d4551;
const unsigned virtio_mmio_device_feature = 0x0;

#define VIRTIO_MMIO_MAX_QUEUE 8

char disk_read(struct mmio_t *unit, unsigned addr) {
  addr -= unit->base;
  char ret = 0;
  unsigned base = addr & 0xFFFFFFFC;
  unsigned offs = addr & 0x00000003;
  switch (base) {
  case VIRTIO_MMIO_MAGIC_VALUE:
    ret = (virtio_mmio_magic >> (8 * offs)) & 0x000000FF;
    break;
  case VIRTIO_MMIO_VERSION:
    ret = (1 >> (8 * offs)) & 0x000000FF; // legacy
    break;
  case VIRTIO_MMIO_DEVICE_ID:
    ret = (2 >> (8 * offs)) & 0x000000FF; // disk
    break;
  case VIRTIO_MMIO_VENDOR_ID:
    ret = (virtio_mmio_vendor_id >> (8 * offs)) & 0x000000FF;
    break;
  case VIRTIO_MMIO_DEVICE_FEATURES:
    ret = (virtio_mmio_device_feature >> (8 * offs)) & 0x000000FF;
    break;
  case VIRTIO_MMIO_QUEUE_NUM_MAX:
    ret = (VIRTIO_MMIO_MAX_QUEUE >> (8 * offs)) & 0x000000FF;
    break;
  case VIRTIO_MMIO_INTERRUPT_STATUS:
    // TODO
    break;
  default:
    ret = 0;
    fprintf(stderr, "mmio (disk): unknown addr read: %08x\n", addr);
    break;
  }
  return ret;
}

typedef struct {
  unsigned addr;
  unsigned len;
  unsigned short flags;
  unsigned short next;
} virtq_desc;

typedef struct {
  unsigned type; // IN or OUT
  unsigned reserved;
  unsigned sector; // TODO unsigned long here
} virtio_blk_req;

// the (entire) avail ring, from the spec.
typedef struct {
  unsigned short flags; // always zero
  unsigned short idx;   // driver will write ring[idx] next
  unsigned short ring[VIRTIO_MMIO_MAX_QUEUE]; // descriptor numbers of chain heads
  unsigned short unused;
} virtq_avail;

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
typedef struct {
  unsigned id;   // index of start of completed descriptor chain
  unsigned len;
} virtq_used_elem;

typedef struct {
  unsigned short flags; // always zero
  unsigned short idx;   // device increments when it adds a ring[] entry
  virtq_used_elem ring[VIRTIO_MMIO_MAX_QUEUE];
} virtq_used;

#define VIRTQ_DESC_F_NEXT 1 // exits next queue
#define VIRTQ_DESC_F_WRITE 2 // mmio writes to the descriptor

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk

#define VIRTQ_DONE 0

#define VIRTQ_STAGE_READ_BLK_REQ 0
#define VIRTQ_STAGE_RW_SECTOR 1
#define VIRTQ_STAGE_COMPLETE 2

#define VIRTQ_DEBUG 0

static void disk_process_queue(disk_t *disk) {
  // run the disk r/w
  unsigned desc_addr = disk->queue_ppn * disk->page_size;
  virtq_desc *desc = (virtq_desc *)memory_get_page(disk->mem, desc_addr);
  virtq_avail *avail = (virtq_avail *)(memory_get_page(disk->mem, desc_addr) + VIRTIO_MMIO_MAX_QUEUE * sizeof(virtq_desc));
  virtq_used *used = (virtq_used *)memory_get_page(disk->mem, desc_addr + disk->page_size);
#if VIRTQ_DEBUG
  fprintf(stderr, "avail flag: %d avail idx: %d used idx: %d ring:", avail->flags, avail->idx, used->idx);
  for (int i = 0; i < VIRTIO_MMIO_MAX_QUEUE; i++) {
    fprintf(stderr, " %d", avail->ring[i]);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "init: -> %d\n", avail->ring[used->idx % VIRTIO_MMIO_MAX_QUEUE]);
#endif
  virtq_desc *current_desc = desc + avail->ring[used->idx % VIRTIO_MMIO_MAX_QUEUE];
  virtio_blk_req *req = NULL;
  for (unsigned i = 0; i < VIRTIO_MMIO_MAX_QUEUE; i++) {
#if VIRTQ_DEBUG
    fprintf(stderr, "Q%u: addr: %08x, len: %08x, flags: %04x, next: %04x\n",
            i, current_desc->addr, current_desc->len, current_desc->flags, current_desc->next
            );
#endif
    int is_write = (current_desc->flags & VIRTQ_DESC_F_WRITE) ? 1 : 0;
    // read from descripted address
    if (i == VIRTQ_STAGE_READ_BLK_REQ && !is_write) {
      req = (virtio_blk_req *)(memory_get_page(disk->mem, current_desc->addr) + (current_desc->addr & disk->page_size_mask));
#if VIRTQ_DEBUG
      fprintf(stderr, "REQ: %s, %08x, %08x\n", (req->type == VIRTIO_BLK_T_IN) ? "read" : "write", req->reserved, req->sector);
#endif
    } else if (i == VIRTQ_STAGE_RW_SECTOR) {
      if (disk->rom->data == NULL) {
        fprintf(stderr, "mmio disk (RW queue): no disk\n");
      } else {
        char *sector = disk->rom->data + (512 * req->sector);
        unsigned dma_base = current_desc->addr;
        if ((req->type == VIRTIO_BLK_T_IN) && is_write) {
          // disk -> memory
          for (unsigned j = 0; j < current_desc->len; j++) {
            char *page = memory_get_page(disk->mem, dma_base + j);
            page[(dma_base + j) & disk->page_size_mask] = sector[j];
            // invalidate cache
            memory_dcache_invalidate_line(disk->mem, dma_base + j);
          }
        } else if ((req->type == VIRTIO_BLK_T_OUT) && !is_write) {
          // memory -> disk
          for (unsigned j = 0; j < current_desc->len; j++) {
            // TODO
            char *page = memory_get_page(disk->mem, dma_base + j);
            sector[j] = page[(dma_base + j) & disk->page_size_mask];
          }
        } else {
          fprintf(stderr, "[MMIO ERROR] invalid sequence\n");
        }
      }
    } else if (i == VIRTQ_STAGE_COMPLETE && is_write) {
      // done
      char *page = memory_get_page(disk->mem, current_desc->addr);
      page[current_desc->addr & disk->page_size_mask] = VIRTQ_DONE;
      memory_dcache_invalidate_line(disk->mem, current_desc->addr);
      // complete
      used->ring[used->idx % VIRTIO_MMIO_MAX_QUEUE].id = avail->ring[used->idx % VIRTIO_MMIO_MAX_QUEUE];
      used->ring[used->idx % VIRTIO_MMIO_MAX_QUEUE].len = i + 1;
      used->idx++; // increment when completed
    }
    // next queue
    if ((current_desc->flags & VIRTQ_DESC_F_NEXT) != VIRTQ_DESC_F_NEXT) {
      break;
    }
#if VIRTQ_DEBUG
    fprintf(stderr, "next: -> %d\n", current_desc->next);
#endif
    current_desc = desc + current_desc->next;
    if (i == VIRTIO_MMIO_MAX_QUEUE - 1) {
      fprintf(stderr, "[MMIO ERROR] EXCEEDS MAX_QUEUE\n");
    }
  }
  // raise interrupt
  disk->queue_notify = 1;
}

void disk_write(struct mmio_t *unit, unsigned addr, char value) {
  addr -= unit->base;
  disk_t *disk = (disk_t *)unit;
  unsigned base = addr & 0xfffffffc;
  unsigned offs = addr & 0x00000003;
  unsigned mask = 0x000000FF << (8 * offs);
  switch (base) {
  case VIRTIO_MMIO_QUEUE_NOTIFY:
    disk->current_queue =
      (disk->current_queue & (~mask)) | ((unsigned char)value << (8 * offs));
    if (offs == 0) {
      disk_process_queue(disk);
    }
    break;
  case VIRTIO_MMIO_QUEUE_PFN:
    disk->queue_ppn =
      (disk->queue_ppn & (~mask)) | ((unsigned char)value << (8 * offs));
    break;
  case VIRTIO_MMIO_GUEST_PAGE_SIZE:
    disk->page_size =
      (disk->page_size & (~mask)) | ((unsigned char)value << (8 * offs));
    // [TODO?] for non 2 power
    disk->page_size_mask = disk->page_size - 1;
    break;
  case VIRTIO_MMIO_DRIVER_FEATURES:
    // [TODO] feature negotiation
    break;
  case VIRTIO_MMIO_QUEUE_NUM:
    disk->queue_num =
      (disk->queue_num & (~mask)) | ((unsigned char)value << (8 * offs));
    break;
  case VIRTIO_MMIO_SELECT_QUEUE:
    disk->current_queue =
      (disk->current_queue & (~mask)) | ((unsigned char)value << (8 * offs));
    break;
  case VIRTIO_MMIO_STATUS:
    // [TODO] we will finally be ready written by 0x0000000F to STATUS
    break;
  case VIRTIO_MMIO_INTERRUPT_ACK:
    // TODO
    break;
  default:
    fprintf(stderr, "mmio (disk): unknown addr write: %08x, %08x\n", addr, value);
    break;
  }
  return;
}

unsigned disk_irq(const disk_t *disk) {
  return disk->queue_notify;
}

void disk_irq_ack(disk_t *disk) {
  disk->queue_notify = 0;
}

void disk_fini(disk_t *disk) {
  rom_fini(disk->rom);
  free(disk->rom);
  return;
}
