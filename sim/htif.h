#ifndef HTIF_H
#define HTIF_H
#include "sim.h"

#ifndef TOHOST_ADDR
#define TOHOST_ADDR 0x80001000
#endif

#ifndef FROMHOST_ADDR
#define FROMHOST_ADDR 0x80001040
#endif

#define SYS_write 64
#define SYS_exit 93
#define SYS_stats 1234

void htif_callback(sim_t *sim, unsigned dcause, unsigned trigger_type, unsigned tdata1, unsigned tdata2, unsigned tdata3);

#endif
