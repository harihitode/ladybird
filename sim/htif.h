#ifndef HTIF_H
#define HTIF_H
#include "sim.h"

// Ordinary called by ecall (a7 holds the system call number)
#define SYS_write 64
#define SYS_exit 93
#define SYS_stats 1234

void htif_callback(sim_t *sim, unsigned dcause, unsigned trigger_type, unsigned tdata1, unsigned tdata2, unsigned tdata3);

#endif
