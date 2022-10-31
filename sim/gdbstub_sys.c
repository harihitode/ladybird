#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sim.h"

#define DBG_SIGHUP 0x01
#define DBG_SIGINT 0x02
#define DBG_SIGQUIT 0x03
#define DBG_SIGILL 0x04
#define DBG_SIGTRAP 0x05
#define DBG_SIGABRT 0x06
#define DBG_SIGPOLL 0x07
#define DBG_SIGFPE 0x08
#define DBG_SIGKILL 0x09
#define DBG_SIGBUS 0x0a
#define DBG_SIGSEGV 0x0b

#define INSTRUCTION_EBREAK 0x00100073
#define INSTRUCTION_CEBREAK 0x00009002

// global
static sim_t *sim;
int sock, client;
int dbg_main(struct dbg_state *state);

int dbg_sys_getc() {
  char ch;
  size_t size = recv(client, &ch, 1, MSG_WAITALL);
  if (size == 0) {
    return EOF;
  } else {
    return (unsigned)ch;
  }
}

int dbg_sys_putchar(int ch) {
  send(client, &ch, 1, 0);
  return 0;
}

int dbg_sys_reg_read(address regno, unsigned *val) {
  *val = sim_read_register(sim, regno);
  return 0;
}

int dbg_sys_reg_write(address regno, unsigned val) {
  sim_write_register(sim, regno, val);
  return 0;
}

int dbg_sys_mem_readb(address addr, char *val) {
  *val = (unsigned char)sim_read_memory(sim, addr);
  return 0;
}

int dbg_sys_mem_writeb(address addr, char val) {
  sim_write_memory(sim, addr, val);
  return 0;
}

int dbg_sys_continue() {
  sim_resume(sim);
  return 0;
}

int dbg_sys_step() {
  sim_single_step(sim);
  return 0;
}

int dbg_sys_kill() {
  return 0;
}

int dbg_sys_set_bw_point(address addr, int type, int kind) {
  struct dbg_break_watch *pp = NULL;
  int find = 0;
  if (type < 0 || type > 5) {
    return 1;
  }
  for (struct dbg_break_watch *p = sim->bw; p != NULL; p = p->next) {
    pp = p;
    if (p->addr == addr && p->type == type && p->kind == kind) {
      find = 1;
      break;
    }
  }
  if (!find) {
    struct dbg_break_watch *np = (struct dbg_break_watch *)malloc(sizeof(struct dbg_break_watch));
    np->addr = addr;
    np->type = type;
    np->kind = kind;
    np->value = 0;
    np->next = NULL;
    if (pp == NULL) {
      sim->bw = np;
    } else {
      pp->next = np;
    }
    switch (type) {
    case 0: // SW Breakpoint
      {
        np->value = np->value | (unsigned char)sim_read_memory(sim, addr);
        np->value = np->value | ((unsigned char)sim_read_memory(sim, addr + 1) << 8);
        np->value = np->value | ((unsigned char)sim_read_memory(sim, addr + 2) << 16);
        np->value = np->value | ((unsigned char)sim_read_memory(sim, addr + 3) << 24);
        printf("break point to %08x, %08x\n", addr, np->value);
        if (np->value & 0x03) {
          // normal
          sim_write_memory(sim, addr, (char)(0x0ff & INSTRUCTION_EBREAK));
          sim_write_memory(sim, addr + 1, (char)(0x0ff & (INSTRUCTION_EBREAK >> 8)));
          sim_write_memory(sim, addr + 2, (char)(0x0ff & (INSTRUCTION_EBREAK >> 16)));
          sim_write_memory(sim, addr + 3, (char)(0x0ff & (INSTRUCTION_EBREAK >> 24)));
        } else {
          // compressed
          sim_write_memory(sim, addr, (char)(0x0ff & INSTRUCTION_CEBREAK));
          sim_write_memory(sim, addr + 1, (char)(0x0ff & (INSTRUCTION_CEBREAK >> 8)));
        }
      }
      break;
    case 1: // HW Breakpoint
      break;
    case 2: // Watchpoint (write)
      break;
    case 3: // Watchpoint (read)
      break;
    default: // Watchpoint (access)
      break;
    }
  }
  return 0;
}

int dbg_sys_rst_bw_point(address addr, int type, int kind) {
  struct dbg_break_watch *pp = NULL;
  for (struct dbg_break_watch *p = sim->bw; p != NULL; p = p->next) {
    if (p->addr == addr && p->type == type && p->kind == kind) {
      if (pp == NULL) {
        sim->bw = NULL;
      } else {
        pp->next = p->next;
      }
      switch (type) {
      case 0: // SW Breakpoint
        printf("break point reset: %08x, %08x\n", addr, p->value);
        if (p->value & 0x03) {
          // normal
          sim_write_memory(sim, addr, (char)(0x0ff & p->value));
          sim_write_memory(sim, addr + 1, (char)(0x0ff & (p->value >> 8)));
          sim_write_memory(sim, addr + 2, (char)(0x0ff & (p->value >> 16)));
          sim_write_memory(sim, addr + 3, (char)(0x0ff & (p->value >> 24)));
        } else {
          // compressed
          sim_write_memory(sim, addr, (char)(0x0ff & p->value));
          sim_write_memory(sim, addr + 1, (char)(0x0ff & (p->value >> 8)));
        }
        break;
      case 1: // HW Breakpoint
        break;
      case 2: // Watchpoint (write)
        break;
      case 3: // Watchpoint (read)
        break;
      default: // Watchpoint (access)
        break;
      }
      free(p);
      break;
    }
    pp = p;
  }
  return 0;
}

char dbg_sys_get_signum() {
  return DBG_SIGTRAP;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE] [DISK FILE]\n", argv[0]);
    return 0;
  }
  sim = (sim_t *)malloc(sizeof(sim_t));
  char addr[32];
  unsigned short port;
  struct sockaddr_in client_addr;
  unsigned client_len = sizeof(client_addr);
  // initialization
  sim_init(sim);
  sim_write_csr(sim, CSR_ADDR_D_CSR, CSR_DCSR_ENABLE_ANY_BREAK);
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
  // config TCP
  port = 12345;
  strncpy(addr, "127.0.0.1", 32);
  {
    struct sockaddr_in server_addr;
    char yes = 1;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, addr, &server_addr.sin_addr.s_addr);
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) < 0) {
      perror("bind");
      goto cleanup;
    }
    if (listen(sock, 1) < 0) {
      perror("listen");
      goto cleanup;
    }
  }
  printf("Debug Server Started: %s:%d\n", addr, port);
  client = accept(sock, (struct sockaddr *)&client_addr, &client_len);
  // start simulation
  dbg_main(sim);
 cleanup:
  sim_fini(sim);
  free(sim);
  close(client);
  close(sock);
  return 0;
}
