#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "sim.h"

sig_atomic_t quit = 0;
int trap = 1;

void shndl(int signum) {
  if (signum == SIGINT) {
    quit = 1;
  }
}

void callback(unsigned trap_code, sim_t *sim) {
  switch (trap_code) {
  case TRAP_CODE_ECALL:
    fprintf(stderr, "ECALL\n");
    trap = 1;
    break;
  case TRAP_CODE_EBREAK:
    fprintf(stderr, "EBREAK\n");
    trap = 1;
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
  // main loop
  char line[128];
  unsigned break_addr = 0;
  while (quit == 0) {
    if (trap) {
      printf("(sim) ");
      if (fgets(line, 128, stdin) == NULL) {
        break;
      }
      if (strncmp(line, "info registers", 6) == 0) {
        for (int i = 0; i < 32; i++) {
          printf("[x%02d]\t0x%08x\n", i, sim_read_register(sim, i));
        }
        printf("[PC]\t0x%08x\n", sim_read_register(sim, 32));
      } else if (strncmp(line, "step", 1) == 0) {
        sim_step(sim);
      } else if (strncmp(line, "c", 1) == 0) {
        trap = 0;
      } else if (strncmp(line, "b", 1) == 0) {
        sscanf(line, "b %08x", &break_addr);
        printf("break point set @ %08x\n", break_addr);
      } else if (strncmp(line, "x", 1) == 0) {
        unsigned addr;
        sscanf(line, "x %08x", &addr);
        printf("%08x (%08x)\n", addr, sim_read_memory(sim, addr));
      }
    } else {
      sim_step(sim);
      if (sim->pc == break_addr) {
        trap = 1;
      }
    }
    // sim_step(sim);
  }
  fprintf(stderr, "@ PC:%08x, INST:%08x\n", sim->pc, sim_read_memory(sim, sim->pc));
  sim_fini(sim);
  free(sim);
  return 0;
}
