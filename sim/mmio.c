#include "mmio.h"
#include "memory.h"
#include <stdint.h>

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

unsigned uart_irq(const uart_t *uart) {
  return 0;
}

void uart_irq_ack(uart_t *uart) {
  return;
}

void uart_write(uart_t *uart, unsigned addr, char value) {
  switch (addr) {
  case 0x00000000:
    if (uart->mode == UART_MODE_DEFAULT) {
      fputc(value, uart->fo);
      fflush(stdout);
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
  disk->mem = NULL;
  disk->current_queue = 0;
  disk->queue_num = 0;
  disk->queue_notify = 0;
  disk->queue_ppn = 0;
  disk->page_size = 0;
}

int disk_load(disk_t *disk, const char *img_path) {
  FILE *fp = NULL;
  if ((fp = fopen(img_path, "r")) == NULL) {
    perror("disk file open");
    return 1;
  }
  fstat(fileno(fp), &disk->img_stat);
  disk->data = (char *)mmap(NULL, disk->img_stat.st_size, PROT_WRITE, MAP_PRIVATE, fileno(fp), 0);
  if (disk->data == MAP_FAILED) {
    perror("disk mmap");
    fclose(fp);
    disk->data = NULL;
    return 1;
  }
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

char disk_read(disk_t *disk, unsigned addr) {
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
    printf("mmio (disk): unknown addr read: %08x\n", addr);
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

static void disk_process_queue(disk_t *disk) {
  // run the disk r/w
  unsigned desc_addr = disk->queue_ppn * disk->page_size - MEMORY_BASE_ADDR_RAM;
  virtq_desc *desc = (virtq_desc *)memory_get_page(disk->mem, desc_addr);
  virtq_avail *avail = (virtq_avail *)(memory_get_page(disk->mem, desc_addr) + VIRTIO_MMIO_MAX_QUEUE * sizeof(virtq_desc));
  virtq_used *used = (virtq_used *)memory_get_page(disk->mem, desc_addr + disk->page_size);
#if 0
  printf("avail flag: %d idx: %d unused: %d ring:", avail->flags, avail->idx, avail->unused);
  for (int i = 0; i < VIRTIO_MMIO_MAX_QUEUE; i++) {
    printf(" %d", avail->ring[i]);
  }
  printf("\n");
#endif
  virtq_desc *current_desc = desc + avail->ring[disk->current_queue];
  virtio_blk_req *req = NULL;
  for (unsigned i = 0; i < VIRTIO_MMIO_MAX_QUEUE; i++) {
#if 0
    printf("Q%u: addr: %08x, len: %08x, flags: %04x, next: %04x\n",
           i, current_desc->addr, current_desc->len, current_desc->flags, current_desc->next
           );
#endif
    int is_write = (current_desc->flags & VIRTQ_DESC_F_WRITE) ? 1 : 0;
    // read from descripted address
    if (i == VIRTQ_STAGE_READ_BLK_REQ && !is_write) {
      req = (virtio_blk_req *)(memory_get_page(disk->mem, current_desc->addr - MEMORY_BASE_ADDR_RAM) + (current_desc->addr & 0x00000FFF));
#if 0
      printf("REQ: %s, %08x, %08x\n", (req->type == VIRTIO_BLK_T_IN) ? "read" : "write", req->reserved, req->sector);
#endif
    }
    if (i == VIRTQ_STAGE_RW_SECTOR) {
      char *sector = disk->data + (512 * req->sector);
      unsigned dma_base = current_desc->addr;
      if ((req->type == VIRTIO_BLK_T_IN) && is_write) {
        // disk -> memory
        for (unsigned j = 0; j < current_desc->len; j++) {
          memory_store(disk->mem, dma_base + j, sector[j]);
        }
      } else if ((req->type == VIRTIO_BLK_T_OUT) && !is_write) {
        // memory -> disk
        for (unsigned j = 0; j < current_desc->len; j++) {
          sector[j] = memory_load(disk->mem, dma_base + j);
        }
      } else {
        printf("[MMIO ERROR] invalid sequence\n");
      }
    }
    if (i == VIRTQ_STAGE_COMPLETE && is_write) {
      // done
      memory_store(disk->mem, current_desc->addr, VIRTQ_DONE);
      used->ring[used->idx].id = avail->ring[disk->current_queue];
      used->ring[used->idx].len = 3;
      used->idx++; // increment when completed
    }
    // next queue
    if ((current_desc->flags & VIRTQ_DESC_F_NEXT) != VIRTQ_DESC_F_NEXT) {
      break;
    }
    current_desc = desc + current_desc->next;
    if (i == VIRTIO_MMIO_MAX_QUEUE - 1) {
      printf("[MMIO ERROR] EXCEEDS MAX_QUEUE\n");
    }
  }
  disk->queue_notify = 1;
}

void disk_write(disk_t *disk, unsigned addr, char value) {
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
    printf("mmio (disk): unknown addr write: %08x, %08x\n", addr, value);
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
  if (disk->data) {
    munmap(disk->data, disk->img_stat.st_size);
  }
  return;
}
