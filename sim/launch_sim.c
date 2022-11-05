#include <stdio.h>
#include <stdlib.h>
#include "sim.h"

#define SYS_write 64
#define SYS_exit 93
#define SYS_stats 1234

#define TOHOST_ADDR 0x80001000
#define FROMHOST_ADDR 0x80001040

void bye() {
  fprintf(stderr, "===================\n");
  fprintf(stderr, "Bye simulator z(--*\n");
  fprintf(stderr, "===================\n");
}

void hello() {
  fprintf(stderr, "===================\n");
  fprintf(stderr, "Hi simulator  !(''*\n");
  fprintf(stderr, "===================\n");
}

void trigger_set(sim_t *sim) {
  // trigger to memory write to 0x80001000 (tohost)
  sim_write_csr(sim, CSR_ADDR_T_SELECT, 0);
  sim_write_csr(sim, CSR_ADDR_T_DATA1, sim_match6(CSR_MATCH6_SELECT_ADDRESS, CSR_MATCH6_TIMING_AFTER, CSR_MATCH6_LOAD | CSR_MATCH6_STORE));
  sim_write_csr(sim, CSR_ADDR_T_DATA2, TOHOST_ADDR);
}

void callback(sim_t *sim) {
  unsigned dcsr = sim_read_csr(sim, CSR_ADDR_D_CSR);
  unsigned cause = (dcsr >> 6) & 0x7;
  if (cause == CSR_DCSR_CAUSE_TRIGGER) {
    dcsr |= CSR_DCSR_MPRV_EN;
    sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
    unsigned magic_mem = 0;
    sim_cache_flush(sim);
    for (int i = 0; i < 4; i++) {
      magic_mem = magic_mem | ((unsigned char)sim_read_memory(sim, TOHOST_ADDR + i) << (i * 8));
    }
    if (magic_mem & 0x1) {
      sim->state = quit;
      fprintf(stderr, "exit with code: %d\n", (int)magic_mem >> 1);
    } else if (magic_mem != 0) {
      unsigned which = 0;
      unsigned arg0 = 0;
      unsigned arg1 = 0;
      unsigned arg2 = 0;
      unsigned test = 0;
      for (int i = 0; i < 4; i++) {
        which = which | ((unsigned char)sim_read_memory(sim, magic_mem + i) << (i * 8));
        arg0 = arg0 | ((unsigned char)sim_read_memory(sim, magic_mem + 8 + i) << (i * 8));
        arg1 = arg1 | ((unsigned char)sim_read_memory(sim, magic_mem + 16 + i) << (i * 8));
        arg2 = arg2 | ((unsigned char)sim_read_memory(sim, magic_mem + 24 + i) << (i * 8));
        test = test | ((unsigned char)sim_read_memory(sim, magic_mem + 32 + i) << (i * 8));
      }
      for (unsigned i = 0; i < arg2; i++) {
        putchar((unsigned char)sim_read_memory(sim, arg1 + i));
      }
      sim_write_memory(sim, FROMHOST_ADDR, 0x1);
    }
    trigger_set(sim);
    dcsr &= ~CSR_DCSR_MPRV_EN;
    sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
  } else if (cause == CSR_DCSR_CAUSE_EBREAK) {
    sim->state = quit;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE] [DISK FILE]\n", argv[0]);
    return 0;
  }
  sim_t *sim = (sim_t *)malloc(sizeof(sim_t));
  // initialization
  sim_init(sim);
  // set ebreak calling debug callback
  sim_write_csr(sim, CSR_ADDR_D_CSR, CSR_DCSR_EBREAK_M | CSR_DCSR_EBREAK_S | CSR_DCSR_EBREAK_U | PRIVILEGE_MODE_M);
  trigger_set(sim);
  sim_debug(sim, callback);
  // load elf file to ram
  if (sim_load_elf(sim, argv[1]) != 0) {
    fprintf(stderr, "error in elf file: %s\n", argv[1]);
    goto cleanup;
  }
  if (argc >= 3) {
    // if you open disk file read only mode, set 1 to the last argument below
    sim_virtio_disk(sim, argv[2], 0);
  }
  if (argc >= 5) {
    sim_uart_io(sim, argv[3], argv[4]);
  } else if (argc >= 4) {
    sim_uart_io(sim, argv[3], NULL);
  }
  hello();
  while (sim->state == running) {
    sim_resume(sim);
  }
  bye();
 cleanup:
  sim_fini(sim);
  free(sim);
  return 0;
}
