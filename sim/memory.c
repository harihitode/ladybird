#include "memory.h"
#include "mmio.h"
#include "sim.h"
#include <stdio.h>
#include <stdlib.h>

#define MEMORY_NONE 0
#define MEMORY_RAM 1
#define MEMORY_UART 2
#define MEMORY_DISK 3

void csr_set_tval(struct csr_t *, unsigned value);
void csr_trap(struct csr_t *, unsigned trap_code);

void memory_init(memory_t *mem) {
  mem->blocks = 0;
  mem->base = NULL;
  mem->block = NULL;
  mem->reserve = NULL;
  mem->csr = NULL;
  mem->uart = (uart_t *)malloc(sizeof(uart_t));
  uart_init(mem->uart);
  mem->disk = (disk_t *)malloc(sizeof(disk_t));
  disk_init(mem->disk);
  disk_load(mem->disk, "../../ladybird_xv6/fs.img");
  mem->vmflag = 0;
  mem->vmbase = 0;
}

static unsigned memory_get_block_id(memory_t *mem, unsigned addr) {
  for (unsigned i = 0; i < mem->blocks; i++) {
    // search blocks allocated for the base addr
    // block size is 4KB
    if (mem->base[i] == (addr & 0xfffff000)) {
      return i;
    }
  }
  unsigned last_block = mem->blocks;
  mem->blocks++; // increment
  mem->base = (unsigned *)realloc(mem->base, mem->blocks * sizeof(unsigned));
  mem->block = (char **)realloc(mem->block, mem->blocks * sizeof(char *));
  mem->reserve = (char *)realloc(mem->reserve, mem->blocks * sizeof(char));
  mem->base[last_block] = (addr & 0xfffff000);
  mem->block[last_block] = (char *)malloc(0x00001000 * sizeof(char));
  mem->reserve[last_block] = 0;
  return last_block;
}

static char ram_read(memory_t *mem, unsigned addr) {
  unsigned bid = memory_get_block_id(mem, addr);
  char *block = mem->block[bid];
  mem->reserve[bid] = 0; // expire
  return block[(addr & 0x00000fff)];
}

static void ram_write(memory_t *mem, unsigned addr, char value) {
  unsigned bid = memory_get_block_id(mem, addr);
  char *block = mem->block[bid];
  mem->reserve[bid] = 0; // expire
  block[(addr & 0x00000fff)] = value;
  return;
}

static unsigned memory_address_translation(memory_t *mem, unsigned addr) {
  if (mem->vmflag == 0) {
    return addr;
  } else {
    unsigned paddr = 0;
    unsigned vpn1 = (addr >> 22) & 0x000003ff;
    unsigned vpn0 = (addr >> 12) & 0x000003ff;
    unsigned pte1 = 0, pte0 = 0;
    unsigned pte1_addr = 0, pte0_addr = 0;
    // level 1
    pte1_addr = mem->vmbase + (vpn1 << 2); // word
    for (unsigned i = 0; i < 4; i++) {
      pte1 = (((unsigned)ram_read(mem, pte1_addr + i - MEMORY_BASE_ADDR_RAM)) << 24) | (pte1 >> 8);
    }
    // level 0
    pte0_addr = ((pte1 >> 10) << 12) + (vpn0 << 2);
    for (unsigned i = 0; i < 4; i++) {
      pte0 = (((unsigned)ram_read(mem, pte0_addr + i - MEMORY_BASE_ADDR_RAM)) << 24) | (pte0 >> 8);
    }
    paddr = ((pte0 & 0xfff00000) << 2) | ((pte0 & 0x000ffc00) << 2) | (addr & 0x00000fff);
#if 0
    if (addr >= 0x10000000 && addr < 0x10000400) {
      printf("VADDR: %08x -> PADDR: %08x\n", addr, paddr);
      printf("pte1: %08x, pte1_addr: %08x, vpn1: %08x\n", pte1, pte1_addr, vpn1);
      printf("pte0: %08x, pte0_addr: %08x, vpn0: %08x\n", pte0, pte0_addr, vpn0);
    }
#endif
    return paddr;
  }
}

static unsigned memory_get_memory_type(memory_t *mem, unsigned addr) {
  unsigned base = (addr & 0xfffff000);
  if (base == MEMORY_BASE_ADDR_UART) {
    return MEMORY_UART;
  } else if (base == MEMORY_BASE_ADDR_DISK) {
    return MEMORY_DISK;
  } else {
    if (base >= MEMORY_BASE_ADDR_RAM) {
      return MEMORY_RAM;
    } else {
      return MEMORY_NONE;
    }
  }
}

char memory_load(memory_t *mem, unsigned addr) {
  unsigned paddr = memory_address_translation(mem, addr);
  char value = 0;
  switch (memory_get_memory_type(mem, paddr)) {
  case MEMORY_UART:
    value = uart_read(mem->uart, paddr - MEMORY_BASE_ADDR_UART);
    break;
  case MEMORY_DISK:
    value = disk_read(mem->disk, paddr - MEMORY_BASE_ADDR_DISK);
    break;
  case MEMORY_RAM:
    value = ram_read(mem, paddr - MEMORY_BASE_ADDR_RAM);
    break;
  default:
    csr_set_tval(mem->csr, addr);
    csr_trap(mem->csr, TRAP_CODE_LOAD_ACCESS_FAULT);
    break;
  }
  return value;
}

void memory_store(memory_t *mem, unsigned addr, char value) {
  unsigned paddr = memory_address_translation(mem, addr);
  switch (memory_get_memory_type(mem, paddr)) {
  case MEMORY_UART:
    uart_write(mem->uart, paddr - MEMORY_BASE_ADDR_UART, value);
    break;
  case MEMORY_DISK:
    disk_write(mem->disk, paddr - MEMORY_BASE_ADDR_DISK, value);
    break;
  case MEMORY_RAM:
    ram_write(mem, paddr - MEMORY_BASE_ADDR_RAM, value);
    break;
  default:
    csr_set_tval(mem->csr, addr);
    csr_trap(mem->csr, TRAP_CODE_STORE_ACCESS_FAULT);
    break;
  }
  return;
}

unsigned memory_load_reserved(memory_t *mem, unsigned addr) {
  unsigned ret;
  char b0, b1, b2, b3;
  b0 = memory_load(mem, addr + 0);
  b1 = memory_load(mem, addr + 1);
  b2 = memory_load(mem, addr + 2);
  b3 = memory_load(mem, addr + 3);
  ret =
    ((b3 << 24) & 0xff000000) | ((b2 << 16) & 0x00ff0000) |
    ((b1 <<  8) & 0x0000ff00) | ((b0 <<  0) & 0x000000ff);
  mem->reserve[memory_get_block_id(mem, addr)] = 1; // TODO: only valid for only 1 hart
  return ret;
}

unsigned memory_store_conditional(memory_t *mem, unsigned addr, unsigned value) {
  const unsigned success = 0;
  const unsigned failure = 1;
  unsigned result;
  result = (mem->reserve[memory_get_block_id(mem, addr)]) ? success : failure;
  if (result == success) {
    memory_store(mem, addr + 0, (char)((value >>  0) & 0x000000ff));
    memory_store(mem, addr + 1, (char)((value >>  8) & 0x000000ff));
    memory_store(mem, addr + 2, (char)((value >> 16) & 0x000000ff));
    memory_store(mem, addr + 3, (char)((value >> 24) & 0x000000ff));
  }
  return result;
}

void memory_atp_on(memory_t *mem, unsigned ppn) {
  mem->vmflag = 1;
  mem->vmbase = ppn << 12;
  return;
}

void memory_atp_off(memory_t *mem) {
  mem->vmflag = 0;
  return;
}

void memory_fini(memory_t *mem) {
  free(mem->base);
  for (unsigned i = 0; i < mem->blocks; i++) {
    free(mem->block[i]);
  }
  free(mem->block);
  free(mem->reserve);
  uart_fini(mem->uart);
  free(mem->uart);
  disk_fini(mem->disk);
  free(mem->disk);
  return;
}
