#include <stdio.h>
#include <stdlib.h>
#include "sim.h"

int quit = 0;

void callback(unsigned trap_code, unsigned value) {
  switch (trap_code) {
  case TRAP_CODE_MRET:
    quit = 1;
    break;
  case TRAP_CODE_INVALID_INSTRUCTION:
    printf("invalid opcode!\n");
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
  while (quit == 0) {
    printf("%08x, %08x\n", sim->pc, sim_read_memory(sim, sim->pc));
    sim_step(sim);
  }
  sim_fini(sim);
  free(sim);
  return 0;
}
