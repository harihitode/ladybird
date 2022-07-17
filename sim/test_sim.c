#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "sim.h"

sig_atomic_t quit = 0;

void shndl(int signum) {
  if (signum == SIGINT) {
    quit = 1;
  }
}

void callback(unsigned trap_code, sim_t *sim) {
  switch (trap_code) {
  case TRAP_CODE_ECALL:
    fprintf(stderr, "ECALL\n");
    quit = 1;
    break;
  case TRAP_CODE_EBREAK:
    fprintf(stderr, "EBREAK\n");
    quit = 1;
    break;
  case TRAP_CODE_INVALID_INSTRUCTION:
    fprintf(stderr, "INVALID INSTRUCTION\n");
    quit = 1;
    break;
  default:
    break;
  }
  return;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "%s [ELF FILE]\n", argv[0]);
    return 0;
  }
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    perror("fopen");
    return 0;
  }
  fclose(fp);
  sim_t *sim;
  sim = (sim_t *)malloc(sizeof(sim_t));
  sim_init(sim, argv[1]);
  sim_trap(sim, callback);
  signal(SIGINT, shndl);
  while (quit == 0) {
    sim_step(sim);
  }
  fprintf(stderr, "@ PC:%08x, INST:%08x\n", sim->pc, sim_read_memory(sim, sim->pc));
  sim_fini(sim);
  free(sim);
  return 0;
}
