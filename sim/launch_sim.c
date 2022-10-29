#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "sim.h"

sig_atomic_t quit = 0;

void shndl(int signum) {
  if (signum == SIGINT) {
    quit = 1;
  }
}

void callback(sim_t *sim) {
  quit = 1;
  return;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE] [DISK FILE]\n", argv[0]);
    return 0;
  }
  sim_t *sim = (sim_t *)malloc(sizeof(sim_t));
  // initialization
  sim_init(sim);
  // set ebreak call callback in any mode
  sim_write_csr(sim, CSR_ADDR_D_CSR, CSR_DCSR_ENABLE_ANY_BREAK);
  sim_debug(sim, callback);
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
  signal(SIGINT, shndl);
  // main loop
  while (quit == 0) {
    sim_step(sim);
  }
 cleanup:
  sim_fini(sim);
  free(sim);
  return 0;
}
