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
    quit = 1;
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
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE] [DISK FILE]\n", argv[0]);
    return 0;
  }
  sim_t *sim = (sim_t *)malloc(sizeof(sim_t));
  sim_init(sim);
  sim_trap(sim, callback);
  if (sim_load_elf(sim, argv[1]) != 0) {
    fprintf(stderr, "error in elf file: %s\n", argv[1]);
    goto cleanup;
  }
  if (argc == 3) {
    // if you open disk file read only mode, set 1 to the last argument below
    sim_virtio_disk(sim, argv[2], 0);
  }
  sim_uart_io(sim, stdin, stdout);
  signal(SIGINT, shndl);
  // main loop
  while (quit == 0) {
    sim_step(sim);
  }
  sim_debug_dump_status(sim);
 cleanup:
  sim_fini(sim);
  free(sim);
  return 0;
}
