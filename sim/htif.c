#include "htif.h"
#include <stdio.h>

void htif_callback(sim_t *sim, unsigned dcause, unsigned trigger_type,
                   unsigned tdata1, unsigned tdata2, unsigned tdata3) {
  unsigned addr = tdata2;
  if (trigger_type == CSR_TDATA1_TYPE_MATCH6 && addr == sim->htif_tohost) {
    unsigned magic_mem = 0;
    sim_cache_flush(sim);
    for (int i = 0; i < 4; i++) {
      magic_mem = magic_mem | ((unsigned char)sim_read_memory(sim, sim->htif_tohost + i) << (i * 8));
    }
    if (magic_mem & 0x1) {
      // SYSTEM HALT REQUEST
      sim->state = quit;
      fprintf(stderr, "@@@ (''* LADYBIRD SIM HTIF MESSAGE @@@\n");
      fprintf(stderr, "@@@ (''* exit with code: %3d       @@@\n", (int)magic_mem >> 1);
    } else if (magic_mem != 0) {
      // SYSTEMCALL EMULATION
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
      if (which == SYS_write) {
        for (unsigned i = 0; i < arg2; i++) {
          putchar(sim_read_memory(sim, arg1 + i));
        }
      } else {
        fprintf(stderr, "@@@ (''* LADYBIRD SIM HTIF MESSAGE @@@\n");
        fprintf(stderr, "@@@ (''* UNKNOWN SYSCALL NO: %3u   @@@\n", which);
      }
      for (int i = 0; i < 8; i++) {
        sim_write_memory(sim, sim->htif_tohost + i, 0x0);
      }
      sim_write_memory(sim, sim->htif_fromhost, 0x1);
    }
  }
}
