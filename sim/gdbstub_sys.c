#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "gdbstub/gdbstub.h"

// global
static sim_t *sim;
int sock, client;

int dbg_sys_getc() {
  char ch;
  recv(client, &ch, 1, MSG_WAITALL);
  return (unsigned)ch;
}

int dbg_sys_putchar(int ch) {
  send(client, &ch, 1, 0);
  return 0;
}

int dbg_sys_mem_readb(address addr, char *val) {
  *val = (unsigned char)sim_read_memory(sim, addr);
  sim_clear_exception(sim); // avoid to stall when illegal address accessed
  return 0;
}

int dbg_sys_mem_writeb(address addr, char val) {
  sim_write_memory(sim, addr, val);
  sim_clear_exception(sim); // avoid to stall when illegal address accessed
  return 0;
}

int dbg_sys_continue() {
  sim_debug_continue(sim);
  dbg_main(sim);
  return 0;
}

int dbg_sys_step() {
  sim_step(sim);
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE] [DISK FILE]\n", argv[0]);
    return 0;
  }
  sim = (sim_t *)malloc(sizeof(sim_t));
  FILE *fi = stdin;
  FILE *fo = stdout;
  struct sockaddr_in server_addr, client_addr;
  int port;
  unsigned client_len;
  // initialization
  sim_init(sim);
  if (sim_load_elf(sim, argv[1]) != 0) {
    fprintf(stderr, "error in elf file: %s\n", argv[1]);
    goto cleanup;
  }
  if (argc >= 3) {
    // if you open disk file read only mode, set 1 to the last argument below
    sim_virtio_disk(sim, argv[2], 0);
  }
  if (argc >= 4) {
    fi = fopen(argv[3], "r");
  }
  if (argc >= 5) {
    fo = fopen(argv[4], "w");
  }
  port = 12345;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
  listen(sock, 1);
  client = accept(sock, (struct sockaddr *)&client_addr, &client_len);
  sim_uart_io(sim, fi, fo);
  dbg_main(sim);
 cleanup:
  close(client);
  close(sock);
  sim_fini(sim);
  free(sim);
  if (fi != stdin && fi) {
    fclose(fi);
  }
  if (fo != stdout && fo) {
    fclose(fo);
  }
  return 0;
}
