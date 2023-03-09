#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include "sim.h"
#include "htif.h"

sim_t *sim;
FILE *logfile = NULL;
long long regwrite[32];
long long regalu[32];
int touch[32];
long long regwrite_total = 0;
long long regread_total = 0;

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

void dump_inst_callback(struct core_step_result *result) {
  printf("%08x, %s\n", result->pc, riscv_get_mnemonic(riscv_decompress(result->inst)));
  return;
}

void debug_callback(sim_t *sim, unsigned dcause, unsigned trigger_type, unsigned tdata1, unsigned tdata2, unsigned tdata3) {
  unsigned addr = tdata2;
  if (dcause == CSR_DCSR_CAUSE_TRIGGER && trigger_type == CSR_TDATA1_TYPE_MATCH6 && addr != TOHOST_ADDR) {
    unsigned access = tdata1 & 0x07;
    printf("break at ");
    if (access == (CSR_MATCH6_STORE | CSR_MATCH6_LOAD)) {
      printf("access ");
    } else if (access == CSR_MATCH6_STORE) {
      printf("store ");
    } else if (access == CSR_MATCH6_LOAD) {
      printf("load ");
    } else {
      printf("exec ");
    }
    printf("%08x\n", addr);
    sim->state = quit;
  } else if (dcause == CSR_DCSR_CAUSE_EBREAK) {
    printf("ebreak\n");
    sim->state = quit;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE]\n", argv[0]);
    return 0;
  }
  sim = (sim_t *)malloc(sizeof(sim_t));
  char log_file_name[128];
  char *uart_in_file_name = NULL;
  char *uart_out_file_name = NULL;
  char *disk_file_name = NULL;
  // initialization
  sim_init(sim);
  // callback for Debug Extension (for ex. lldb)
  sim_set_debug_callback(sim, debug_callback);
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--htif") == 0) {
      sim_set_debug_callback(sim, htif_callback);
      sim_set_write_trigger(sim, TOHOST_ADDR);
    } else if (strcmp(argv[i], "--ebreak") == 0) {
      sim_write_csr(sim, CSR_ADDR_D_CSR, CSR_DCSR_EBREAK_M | CSR_DCSR_EBREAK_S | CSR_DCSR_EBREAK_U | PRIVILEGE_MODE_M);
    } else if (strcmp(argv[i], "--dump") == 0) {
      sim_set_step_callback(sim, dump_inst_callback);
    } else if (strcmp(argv[i], "--uart-in") == 0) {
      i++;
      if (i < argc) {
        uart_in_file_name = argv[i];
      }
    } else if (strcmp(argv[i], "--uart-out") == 0) {
      i++;
      if (i < argc) {
        uart_out_file_name = argv[i];
      }
    } else if (strcmp(argv[i], "--disk") == 0) {
      i++;
      if (i < argc) {
        disk_file_name = argv[i];
      }
    }
  }
  // load elf file to ram
  if (sim_load_elf(sim, argv[1]) != 0) {
    fprintf(stderr, "error in elf file: %s\n", argv[1]);
    goto cleanup;
  }
  // if you open disk file read only mode, set 1 to the last argument below
  sim_virtio_disk(sim, disk_file_name, 0);
  sim_uart_io(sim, uart_in_file_name, uart_out_file_name);

  regwrite_total = 0;
  for (int i = 0; i < 32; i++) {
    regwrite[i] = -1;
    regalu[i] = 0;
  }
  sprintf(log_file_name, "%s.log", basename(argv[1]));
  logfile = fopen(log_file_name, "w");
  fprintf(logfile, "# mnemonic regno R/W after_last_write use_count_by_alu\n");
  // hello();
  sim_write_register(sim, REG_A0, 0);
  sim_write_register(sim, REG_A1, DEVTREE_ROM_ADDR);
  while (sim->state == running) {
    sim_resume(sim);
  }
  // bye();
  fprintf(logfile, "# total_regwrite %lld total_regread %lld\n", regwrite_total, regread_total);
 cleanup:
  sim_fini(sim);
  free(sim);
  fclose(logfile);
  return 0;
}
