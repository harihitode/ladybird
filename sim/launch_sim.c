#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "sim.h"

#define SYS_write 64
#define SYS_exit 93
#define SYS_stats 1234

#ifndef TOHOST_ADDR
#define TOHOST_ADDR 0x80001000
#endif

#ifndef FROMHOST_ADDR
#define FROMHOST_ADDR 0x80001040
#endif

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

void console_action(sim_t *sim) {
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
    char ch = sim_read_memory(sim, arg1);
    putchar(ch);
    // for (unsigned i = 0; i < arg2; i++) {
    //   putchar((unsigned char)sim_read_memory(sim, arg1 + i));
    // }
    for (int i = 0; i < 8; i++) {
      sim_write_memory(sim, TOHOST_ADDR + i, 0x0);
    }
    sim_write_memory(sim, FROMHOST_ADDR, 0x1);
  }
}

void step_handler(struct core_step_result *result) {
  printf("%08x, %s\n", result->pc, riscv_get_mnemonic(riscv_decompress(result->inst)));
  return;
}

void debug_handler(sim_t *sim) {
  unsigned dcsr = sim_read_csr(sim, CSR_ADDR_D_CSR);
  unsigned cause = (dcsr >> 6) & 0x7;
  if (cause == CSR_DCSR_CAUSE_TRIGGER) {
    int trigger_index = sim_get_trigger_fired(sim);
    if (trigger_index >= 0) {
      dcsr |= CSR_DCSR_MPRV_EN;
      // [M mode] access to MMU -> ON
      sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
      sim_write_csr(sim, CSR_ADDR_T_SELECT, trigger_index);
      unsigned tdata1 = sim_read_csr(sim, CSR_ADDR_T_DATA1);
      switch (sim_get_trigger_type(tdata1)) {
      case CSR_TDATA1_TYPE_MATCH6: {
        unsigned addr = sim_read_csr(sim, CSR_ADDR_T_DATA2);
        if (addr == TOHOST_ADDR) {
          console_action(sim);
        } else {
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
        }
      }
      default:
        break;
      }
      // [M mode] access to MMU -> OFF
      dcsr &= ~CSR_DCSR_MPRV_EN;
      sim_write_csr(sim, CSR_ADDR_D_CSR, dcsr);
    }
    sim_rst_trigger_hit(sim);
  } else if (cause == CSR_DCSR_CAUSE_EBREAK) {
    sim->state = quit;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE] [DISK FILE]\n", argv[0]);
    return 0;
  }
  sim = (sim_t *)malloc(sizeof(sim_t));
  char log_file_name[128];
  // initialization
  sim_init(sim);
  // callback for Debug Extension (for ex. lldb)
  sim_set_debug_callback(sim, debug_handler);
  // [option] dump cycle instructions (noisy)
  // sim_set_step_callback(sim, step_handler);

  // [option] set ebreak calling debug callback
  // sim_write_csr(sim, CSR_ADDR_D_CSR, CSR_DCSR_EBREAK_M | CSR_DCSR_EBREAK_S | CSR_DCSR_EBREAK_U | PRIVILEGE_MODE_M);
  sim_set_write_trigger(sim, TOHOST_ADDR);

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
