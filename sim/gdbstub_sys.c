#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "gdbstub_sys.h"

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
int dbg_main(struct dbg_state *state);

int dbg_sys_getc(struct dbg_state *state) {
  char ch;
  size_t size = recv(state->client, &ch, 1, MSG_WAITALL);
  if (size == 0) {
    return EOF;
  } else {
    return (unsigned)ch;
  }
}

int dbg_sys_putchar(struct dbg_state *state, int ch) {
  send(state->client, &ch, 1, 0);
  return 0;
}

int dbg_sys_reg_read(struct dbg_state *state, address regno, unsigned *val) {
  *val = sim_read_register(state->sim, regno);
  return 0;
}

int dbg_sys_reg_write(struct dbg_state *state, address regno, unsigned val) {
  sim_write_register(state->sim, regno, val);
  return 0;
}

int dbg_sys_mem_readb(struct dbg_state *state, address addr, char *val) {
  *val = (unsigned char)sim_read_memory(state->sim, addr);
  return 0;
}

int dbg_sys_mem_writeb(struct dbg_state *state, address addr, char val) {
  sim_write_memory(state->sim, addr, val);
  return 0;
}

int dbg_sys_continue(struct dbg_state *state) {
  sim_resume(state->sim);
  return 0;
}

int dbg_sys_step(struct dbg_state *state) {
  sim_single_step(state->sim);
  return 0;
}

int dbg_sys_kill(struct dbg_state *state) {
  state->sim->state = quit;
  return 0;
}

int dbg_sys_set_bw_point(struct dbg_state *state, address addr, int type, int kind) {
  if (type < 0 || type > 5) {
    return -1;
  }
  switch (type) {
  case 0: // SW Breakpoint
    {
      int new_id = 0;
      for (int i = 0; i < state->n_bp; i++) {
        if (state->bp[i].valid && state->bp[i].addr == addr) {
          return 0;
        }
      }
      for (new_id = 0; new_id < state->n_bp; new_id++) {
        if (!state->bp[new_id].valid) break;
      }
      unsigned opcode = sim_read_memory(state->sim, addr) & 0x7f;
      state->bp[new_id].valid = 1;
      state->bp[new_id].addr = addr;
      state->bp[new_id].inst = 0;
      if ((opcode & 0x3) == 0x3) {
        // normal
        for (int i = 0; i < 4; i++) {
          state->bp[new_id].inst |= (unsigned char)sim_read_memory(state->sim, addr + i) << (i * 8);
          sim_write_memory(state->sim, addr + i, (char)(INSTRUCTION_EBREAK >> (i * 8)));
        }
      } else {
        // compressed
        for (int i = 0; i < 2; i++) {
          state->bp[new_id].inst |= (unsigned char)sim_read_memory(state->sim, addr + i) << (i * 8);
          sim_write_memory(state->sim, addr + i, (char)(INSTRUCTION_CEBREAK >> (i * 8)));
        }
      }
    }
    return 0;
  case 1: // HW Breakpoint
    return sim_set_exec_trigger(state->sim, addr, type, kind);
  case 2: // Watchpoint (write)
    return sim_set_write_trigger(state->sim, addr, type, kind);
  case 3: // Watchpoint (read)
    return sim_set_read_trigger(state->sim, addr, type, kind);
  default: // Watchpoint (access)
    return sim_set_access_trigger(state->sim, addr, type, kind);
  }
}

int dbg_sys_rst_bw_point(struct dbg_state *state, address addr, int type, int kind) {
  if (type < 0 || type > 5) {
    return -1;
  }
  switch (type) {
  case 0: // SW Breakpoint
    for (int i = 0; i < state->n_bp; i++) {
      if (state->bp[i].valid && state->bp[i].addr == addr) {
        if ((state->bp[i].inst & 0x3) == 0x3) {
          // normal
          for (int j = 0; j < 4; j++) {
            sim_write_memory(state->sim, addr + j, (char)(state->bp[i].inst >> (j * 8)));
          }
        } else {
          // compressed
          for (int j = 0; j < 2; j++) {
            sim_write_memory(state->sim, addr + j, (char)(state->bp[i].inst >> (j * 8)));
          }
        }
      }
    }
    return 0;
  case 1: // HW Breakpoint
    return sim_rst_exec_trigger(state->sim, addr, type, kind);
  case 2: // Watchpoint (write)
    return sim_rst_write_trigger(state->sim, addr, type, kind);
  case 3: // Watchpoint (read)
    return sim_rst_read_trigger(state->sim, addr, type, kind);
  default: // Watchpoint (access)
    return sim_rst_access_trigger(state->sim, addr, type, kind);
  }
}

char dbg_sys_get_signum(struct dbg_state *state) {
  return DBG_SIGTRAP;
}

char *dbg_sys_get_reginfo(struct dbg_state *state, unsigned regno) {
  return state->sim->reginfo[regno];
}

char *dbg_sys_get_triple(struct dbg_state *state) {
  return state->sim->triple;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "%s [ELF FILE] [DISK FILE]\n", argv[0]);
    return 0;
  }
  struct dbg_state *state = (struct dbg_state *)malloc(sizeof(struct dbg_state));
  state->sim = (sim_t *)malloc(sizeof(sim_t));
  state->n_bp = 128;
  state->bp = (struct breakpoint *)calloc(state->n_bp, sizeof(struct breakpoint));
  char addr[32];
  unsigned short port;
  struct sockaddr_in client_addr;
  unsigned client_len = sizeof(client_addr);
  // initialization
  sim_init(state->sim);
  // set ebreak calling debug callback
  sim_write_csr(state->sim, CSR_ADDR_D_CSR, CSR_DCSR_EBREAK_M | CSR_DCSR_EBREAK_S | CSR_DCSR_EBREAK_U | PRIVILEGE_MODE_M);
  if (sim_load_elf(state->sim, argv[1]) != 0) {
    fprintf(stderr, "error in elf file: %s\n", argv[1]);
    goto cleanup;
  }
  if (argc >= 3) {
    // if you open disk file read only mode, set 1 to the last argument below
    sim_virtio_disk(state->sim, argv[2], 0);
  }
  if (argc >= 5) {
    sim_uart_io(state->sim, argv[3], argv[4]);
  } else if (argc >= 4) {
    sim_uart_io(state->sim, argv[3], NULL);
  }
  // config TCP
  port = 12345;
  strncpy(addr, "127.0.0.1", 32);
  {
    struct sockaddr_in server_addr;
    char yes = 1;
    state->sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(state->sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, addr, &server_addr.sin_addr.s_addr);
    if (bind(state->sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) < 0) {
      perror("bind");
      goto cleanup;
    }
    if (listen(state->sock, 1) < 0) {
      perror("listen");
      goto cleanup;
    }
  }
  printf("Debug Server Started: %s:%d\n", addr, port);
  state->client = accept(state->sock, (struct sockaddr *)&client_addr, &client_len);
  // start simulation
  dbg_main(state);
 cleanup:
  sim_fini(state->sim);
  free(state->sim);
  free(state->bp);
  close(state->client);
  close(state->sock);
  free(state);
  return 0;
}
