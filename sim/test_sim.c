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

void callback(sim_t *sim) {
  unsigned trap_code = sim_get_trap_code(sim);
  switch (trap_code) {
  case TRAP_CODE_ENVIRONMENT_CALL_M:
    // fprintf(stderr, "ECALL (Machine Mode) SYSCALL NO.%d\n", sim_read_register(sim, 17));
    break;
  case TRAP_CODE_ENVIRONMENT_CALL_S:
    // fprintf(stderr, "ECALL (Supervisor Mode) SYSCALL NO.%d\n", sim_read_register(sim, 17));
    break;
  case TRAP_CODE_ENVIRONMENT_CALL_U:
    // fprintf(stderr, "ECALL (User Mode) SYSCALL NO.%d\n", sim_read_register(sim, 17));
    break;
  case TRAP_CODE_BREAKPOINT:
    fprintf(stderr, "BREAKPOINT\n");
    trap = 1;
    break;
  case TRAP_CODE_ILLEGAL_INSTRUCTION:
    fprintf(stderr, "ILLEGAL INSTRUCTION\n");
    quit = 1;
    break;
  case TRAP_CODE_STORE_ACCESS_FAULT:
    fprintf(stderr, "Store/AMO Access Fault: %08x\n", sim_get_trap_value(sim));
    quit = 1;
    break;
  case TRAP_CODE_LOAD_ACCESS_FAULT:
    fprintf(stderr, "Load Access Fault: %08x\n", sim_get_trap_value(sim));
    quit = 1;
    break;
  case TRAP_CODE_M_TIMER_INTERRUPT:
    // fprintf(stderr, "Timer Interrupt [M]\n");
    break;
  case TRAP_CODE_S_TIMER_INTERRUPT:
    // fprintf(stderr, "Timer Interrupt [S]\n");
    break;
  case TRAP_CODE_S_SOFTWARE_INTERRUPT:
    // fprintf(stderr, "SW Interrupt [S]\n");
    break;
  case TRAP_CODE_S_EXTERNAL_INTERRUPT:
    // fprintf(stderr, "EX Interrupt [S]\n");
    break;
  default:
    fprintf(stderr, "Unknown Trap: %08x\n", trap_code);
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
  unsigned break_on = 0;
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
        break_on = 1;
        printf("break point set @ %08x\n", break_addr);
      } else if (strncmp(line, "x", 1) == 0) {
        unsigned addr;
        sscanf(line, "x %08x", &addr);
        printf("%08x (%08x)\n", addr, sim_read_memory(sim, addr));
      }
    } else {
      sim_step(sim);
      if (sim->pc == break_addr && break_on) {
        printf("BREAK!\n");
        break_on = 0;
        trap = 1;
      }
    }
  }
  sim_debug_dump_status(sim);
  sim_fini(sim);
  free(sim);
  return 0;
}
