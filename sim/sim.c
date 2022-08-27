#include "sim.h"
#include "elfloader.h"
#include "memory.h"
#include "csr.h"
#include "mmio.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_GPR 32
#define NUM_FPR 32

void sim_init(sim_t *sim) {
  // clear gpr
  sim->gpr = (unsigned *)calloc(NUM_GPR, sizeof(unsigned));
  // init csr
  sim->csr = (csr_t *)malloc(sizeof(csr_t));
  csr_init(sim->csr);
  csr_set_sim(sim->csr, sim);
  // init memory
  sim->mem = (memory_t *)malloc(sizeof(memory_t));
  memory_init(sim->mem);
  memory_set_sim(sim->mem, sim);
  sim->pc = 0;
  return;
}

int sim_load_elf(sim_t *sim, const char *elf_path) {
  int ret = 1;
  // init elf loader
  sim->elf = (elf_t *)malloc(sizeof(elf_t));
  elf_init(sim->elf, elf_path);
  if (sim->elf->status != ELF_STATUS_LOADED) {
    goto cleanup;
  }
  // program load to memory
  for (unsigned i = 0; i < sim->elf->programs; i++) {
    for (unsigned j = 0; j < sim->elf->program_size[i]; j++) {
      memory_store(sim->mem, sim->elf->program_base[i] + j, sim->elf->program[i][j], 1, 0);
    }
  }
  // set entry program counter
  sim->pc = sim->elf->entry_address;
  ret = 0;
 cleanup:
  elf_fini(sim->elf);
  free(sim->elf);
  sim->elf = NULL;
  return ret;
}

void sim_fini(sim_t *sim) {
  free(sim->gpr);
  memory_fini(sim->mem);
  free(sim->mem);
  csr_fini(sim->csr);
  free(sim->csr);
  return;
}

void sim_trap(sim_t *sim, void (*callback)(sim_t *)) {
  sim->csr->trap_handler = callback;
  return;
}

#define OPCODE_LOAD 0x00000003
#define OPCODE_MISC_MEM 0x0000000F
#define OPCODE_OP_IMM 0x00000013
#define OPCODE_AUIPC 0x00000017
#define OPCODE_STORE 0x00000023
#define OPCODE_AMO 0x0000002f
#define OPCODE_OP 0x00000033
#define OPCODE_LUI 0x00000037
#define OPCODE_BRANCH 0x00000063
#define OPCODE_JALR 0x00000067
#define OPCODE_JAL 0x0000006F
#define OPCODE_SYSTEM 0x00000073

static unsigned get_opcode(unsigned inst) { return inst & 0x0000007f; }
static unsigned get_rs1(unsigned inst) { return (inst >> 15) & 0x0000001f; }
static unsigned get_rs2(unsigned inst) { return (inst >> 20) & 0x0000001f; }
static unsigned get_rd(unsigned inst) { return (inst >> 7) & 0x0000001f; }
static unsigned get_funct3(unsigned inst) { return (inst >> 12) & 0x00000007; }
static unsigned get_funct5(unsigned inst) { return (inst >> 27) & 0x0000001f; }
static unsigned get_funct7(unsigned inst) { return (inst >> 25) & 0x0000007f; }
static unsigned get_branch_offset(unsigned inst) {
  return ((((int)inst >> 19) & 0xfffff000) |
          (((inst >> 25) << 5) & 0x000007e0) |
          (((inst >> 7) & 0x00000001) << 11) | ((inst >> 7) & 0x0000001e));
}
static unsigned get_jalr_offset(unsigned inst) {
  return ((int)inst >> 20);
}
static unsigned get_jal_offset(unsigned inst) {
  return ((((int)inst >> 11) & 0xfff00000) | (inst & 0x000ff000) |
          (((inst >> 20) & 0x00000001) << 11) |
          ((inst >> 20) & 0x000007fe));
}
static unsigned get_store_offset(unsigned inst) {
  return ((((int)inst >> 25) << 5) | ((inst >> 7) & 0x0000001f));
}
static unsigned get_load_offset(unsigned inst) {
  return ((int)inst >> 20);
}
static unsigned get_csr_addr(unsigned inst) {
  return ((inst >> 20) & 0x00000fff);
}

void sim_step(sim_t *sim) {
  // fetch
  unsigned inst = memory_load_instruction(sim->mem, sim->pc);
  unsigned result = 0;
  unsigned opcode;
  unsigned rs1, rs2;
  unsigned src1, src2;
  if (sim->csr->exception) {
    goto csr_update;
  }
  opcode = get_opcode(inst);
  rs1 = get_rs1(inst);
  rs2 = get_rs2(inst);
  // read
  src1 = sim_read_register(sim, rs1);
  src2 = sim_read_register(sim, rs2);
  // exec
  switch (opcode) {
  case OPCODE_OP_IMM:
    switch (get_funct3(inst)) {
    case 0x2:
    case 0x5: // for shift
      src2 = ((int)inst >> 20) & 0x0000001f;
      break;
    default:
      src2 = ((int)inst >> 20);
      break;
    }
    // fall-through
  case OPCODE_OP:
    if (opcode == OPCODE_OP && get_funct7(inst) == 0x00000001) {
      // MUL/DIV
      switch (get_funct3(inst)) {
      case 0x0:
        result = ((long long)src1 * (long long)src2) & 0xffffffff;
        break;
      case 0x1: // MULH (extended: signed * signedb)
        result = ((long long)src1 * (long long)src2) >> 32;
        break;
      case 0x2: // MULHSU (extended: signed * unsigned)
        result = ((long long)src1 * (unsigned long long)src2) >> 32;
        break;
      case 0x3: // MULHU (extended: unsigned * unsigned)
        result = ((unsigned long long)src1 * (unsigned long long)src2) >> 32;
        break;
      case 0x4: // DIV
        result = (int)src1 / (int)src2;
        break;
      case 0x5: // DIVU
        result = (unsigned)src1 / (unsigned)src2;
        break;
      case 0x6: // REM
        result = (int)src1 % (int)src2;
        break;
      case 0x7: // REMU
        result = (unsigned)src1 % (unsigned)src2;
        break;
      }
    } else {
      switch (get_funct3(inst)) {
      case 0x0: // ADD, SUB, ADDI
        if ((get_opcode(inst) == OPCODE_OP) && (inst & 0x40000000)) {
          result = src1 - src2; // SUB
        } else {
          result = src1 + src2; // ADD, ADDI
        }
        break;
      case 0x1: // SLL
        result = (src1 << (src2 & 0x0000001F));
        break;
      case 0x2: // Set Less-Than
        result = ((int)src1 < (int)src2) ? 1 : 0;
        break;
      case 0x3: // Set Less-Than Unsigned
        result = (src1 < src2) ? 1 : 0;
        break;
      case 0x4: // Logical XOR
        result = src1 ^ src2;
        break;
      case 0x5: // SRA, SRL
        if (inst & 0x40000000) {
          result = (int)src1 >> (src2 & 0x0000001F);
        } else {
          result = src1 >> (src2 & 0x0000001F);
        }
        break;
      case 0x6: // Logical OR
        result = src1 | src2;
        break;
      case 0x7: // Logical AND
        result = src1 & src2;
        break;
      }
    }
    break;
  case OPCODE_AUIPC:
    result = sim->pc + (inst & 0xfffff000);
    break;
  case OPCODE_LUI:
    result = (inst & 0xfffff000);
    break;
  case OPCODE_JALR:
  case OPCODE_JAL:
    // for link address
    result = sim->pc + 4;
    break;
  case OPCODE_STORE:
    switch (get_funct3(inst)) {
    case 0x0:
      memory_store(sim->mem, src1 + get_store_offset(inst), src2, 1, 0);
      break;
    case 0x1:
      memory_store(sim->mem, src1 + get_store_offset(inst), src2, 2, 0);
      break;
    case 0x2:
      memory_store(sim->mem, src1 + get_store_offset(inst), src2, 4, 0);
      break;
    default:
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    break;
  case OPCODE_LOAD:
    switch (get_funct3(inst)) {
    case 0x0: // singed ext byte
      result = (int)((char)memory_load(sim->mem, src1 + get_load_offset(inst), 1, 0));
      break;
    case 0x1: // signed ext half
      result = (int)((short)memory_load(sim->mem, src1 + get_load_offset(inst), 2, 0));
      break;
    case 0x2:
      result = memory_load(sim->mem, src1 + get_load_offset(inst), 4, 0);
      break;
    case 0x4:
      result = (unsigned char)memory_load(sim->mem, src1 + get_load_offset(inst), 1, 0);
      break;
    case 0x5:
      result = (unsigned short)memory_load(sim->mem, src1 + get_load_offset(inst), 2, 0);
      break;
    default:
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    break;
  case OPCODE_AMO:
    switch (get_funct5(inst)) {
    case 0x002: // Load Reserved
      result = memory_load_reserved(sim->mem, src1);
      break;
    case 0x003: // Store Conditional
      result = memory_store_conditional(sim->mem, src1, src2);
      break;
    case 0x000: // AMOADD
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, result + src2);
      break;
    case 0x001: // AMOSWAP
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, src2);
      break;
    case 0x004: // AMOXOR
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, result ^ src2);
      break;
    case 0x008: // AMOOR
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, result | src2);
      break;
    case 0x00c: // AMOAND
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, result & src2);
      break;
    case 0x010: // AMOMIN
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, ((int)result < (int)src2) ? result : src2);
      break;
    case 0x014: // AMOMAX
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, ((int)result < (int)src2) ? src2 : result);
      break;
    case 0x018: // AMOMINU
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, (result < src2) ? result : src2);
      break;
    case 0x01c: // AMOMAXU
      result = sim_read_memory(sim, src1);
      sim_write_memory(sim, src1, (result < src2) ? src2 : result);
      break;
    default:
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    break;
  case OPCODE_MISC_MEM:
    // TODO: currently no implementataion
    break;
  case OPCODE_SYSTEM:
    // CSR OPERATIONS
    switch (get_funct3(inst)) {
    case 0x1: // READ_WRITE
      result = csr_csrrw(sim->csr, get_csr_addr(inst), src1);
      break;
    case 0x2: // READ_SET
      result = csr_csrrs(sim->csr, get_csr_addr(inst), src1);
      break;
    case 0x3: // READ_CLEAR
      result = csr_csrrc(sim->csr, get_csr_addr(inst), src1);
      break;
    case 0x4: // Hypervisor Extension
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    case 0x5: // READ_WRITE (imm)
      result = csr_csrrw(sim->csr, get_csr_addr(inst), rs1);
      break;
    case 0x6: // READ_SET (imm)
      result = csr_csrrs(sim->csr, get_csr_addr(inst), rs1);
      break;
    case 0x7: // READ_CLEAR (imm)
      result = csr_csrrc(sim->csr, get_csr_addr(inst), rs1);
      break;
    default: // OTHER SYSTEM OPERATIONS (ECALL, EBREAK, MRET, etc.)
      switch (get_funct7(inst)) {
      case 0x00:
        if (rs2 == 0) {
          if (sim->csr->mode == PRIVILEGE_MODE_M) {
            csr_exception(sim->csr, TRAP_CODE_ENVIRONMENT_CALL_M);
          } else if (sim->csr->mode == PRIVILEGE_MODE_S) {
            csr_exception(sim->csr, TRAP_CODE_ENVIRONMENT_CALL_S);
          } else {
            csr_exception(sim->csr, TRAP_CODE_ENVIRONMENT_CALL_U);
          }
        } else if (rs2 == 1) {
          csr_exception(sim->csr, TRAP_CODE_BREAKPOINT);
        } else {
          csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
        }
        break;
      case 0x18:
        if (rs2 == 2) {
          // MRET
          csr_trapret(sim->csr);
        } else {
          csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
        }
        break;
      case 0x08:
        if (rs2 == 2) {
          // SRET
          csr_trapret(sim->csr);
        } else {
          csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
        }
        break;
      case 0x09:
        // SFENCE.VMA
        memory_tlb_clear(sim->mem);
        break;
      default:
        csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
        break;
      }
    }
    break;
  case OPCODE_BRANCH:
    switch (get_funct3(inst)) {
    case 0x0:
      result = (src1 == src2) ? 1 : 0;
      break;
    case 0x1:
      result = (src1 != src2) ? 1 : 0;
      break;
    case 0x4:
      result = ((int)src1 < (int)src2) ? 1 : 0;
      break;
    case 0x5:
      result = ((int)src1 >= (int)src2) ? 1 : 0;
      break;
    case 0x6:
      result = (src1 < src2) ? 1 : 0;
      break;
    case 0x7:
      result = (src1 >= src2) ? 1 : 0;
      break;
    default:
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    break;
  default:
    // invalid opcode
    csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
    break;
  }
  if (sim->csr->exception || sim->csr->trapret) {
    goto csr_update;
  }
  // commit
  unsigned rd;
  switch (opcode) {
  case OPCODE_OP_IMM:
  case OPCODE_OP:
  case OPCODE_AUIPC:
  case OPCODE_LUI:
  case OPCODE_LOAD:
  case OPCODE_AMO:
    // when rd = 0, SYSTEM is a csr instruction.
  case OPCODE_SYSTEM:
    if ((rd = get_rd(inst)) != 0) {
      sim_write_register(sim, rd, result);
    }
    sim->pc += 4;
    break;
  case OPCODE_BRANCH:
    if (result) {
      sim->pc = sim->pc + get_branch_offset(inst);
    } else {
      sim->pc = sim->pc + 4;
    }
    break;
  case OPCODE_JAL:
    if ((rd = get_rd(inst)) != 0) {
      sim_write_register(sim, rd, result);
    }
    sim->pc = sim->pc + get_jal_offset(inst);
    break;
  case OPCODE_JALR:
    if ((rd = get_rd(inst)) != 0) {
      sim_write_register(sim, rd, result);
    }
    sim->pc = src1 + get_jalr_offset(inst);
    break;
  default:
    sim->pc += 4;
    break;
  }
 csr_update:
  csr_cycle(sim->csr, 1); // 1.0 ipc
  return;
}

unsigned sim_read_register(sim_t *sim, unsigned regno) {
  if (regno == 0) {
    return 0;
  } else if (regno < NUM_GPR) {
    return sim->gpr[regno];
  } else {
    return sim->pc;
  }
}

void sim_write_register(sim_t *sim, unsigned regno, unsigned value) {
  if (regno < NUM_GPR) {
    sim->gpr[regno] = value;
  }
  return;
}

unsigned sim_read_memory(sim_t *sim, unsigned addr) {
  unsigned ret = memory_load(sim->mem, addr, 4, 0);
  return ret;
}

void sim_write_memory(sim_t *sim, unsigned addr, unsigned value) {
  memory_store(sim->mem, addr, value, 4, 0);
  return;
}

unsigned sim_get_trap_code(sim_t *sim) {
  if (sim->csr->mode == PRIVILEGE_MODE_M) {
    return sim->csr->mcause;
  } else {
    return sim->csr->scause;
  }
}

unsigned sim_get_trap_value(sim_t *sim) {
  return csr_get_tval(sim->csr);
}

unsigned sim_get_epc(sim_t *sim) {
  if (sim->csr->mode == PRIVILEGE_MODE_M) {
    return sim->csr->mepc;
  } else {
    return sim->csr->sepc;
  }
}

int sim_virtio_disk(sim_t *sim, const char *img_path, int mode) {
  disk_load(sim->mem->disk, img_path, mode);
  return 0;
}

int sim_uart_io(sim_t *sim, FILE *in, FILE *out) {
  uart_set_io(sim->mem->uart, in, out);
  return 0;
}

void sim_debug_dump_status(sim_t *sim) {
  fprintf(stderr, "MODE: ");
  switch (sim->csr->mode) {
  case PRIVILEGE_MODE_M:
    fprintf(stderr, "Machine\n");
    break;
  case PRIVILEGE_MODE_S:
    fprintf(stderr, "Supervisor\n");
    break;
  case PRIVILEGE_MODE_U:
    fprintf(stderr, "User\n");
    break;
  default:
    fprintf(stderr, "unknown: %d\n", sim->csr->mode);
    break;
  }
  fprintf(stderr, "PC: %08x, (RAM: %08x, %08x)\n", sim->pc, memory_address_translation(sim->mem, sim->pc, 0), memory_load_instruction(sim->mem, sim->pc));
  fprintf(stderr, "STATUS: %08x\n", csr_csrr(sim->csr, CSR_ADDR_M_STATUS));
  fprintf(stderr, "MIP: %08x\n", csr_csrr(sim->csr, CSR_ADDR_M_IP));
  fprintf(stderr, "MIE: %08x\n", csr_csrr(sim->csr, CSR_ADDR_M_IE));
  fprintf(stderr, "SIP: %08x\n", csr_csrr(sim->csr, CSR_ADDR_S_IP));
  fprintf(stderr, "SIE: %08x\n", csr_csrr(sim->csr, CSR_ADDR_S_IE));
  fprintf(stderr, "Instruction Count: %lu\n", sim->csr->instret);
  fprintf(stderr, "ICACHE: hit: %f%%, [%lu/%lu]\n", (double)sim->mem->icache->hit_count / sim->mem->icache->access_count, sim->mem->icache->hit_count, sim->mem->icache->access_count);
  fprintf(stderr, "DCACHE: hit: %f%%, [%lu/%lu]\n", (double)sim->mem->dcache->hit_count / sim->mem->dcache->access_count, sim->mem->dcache->hit_count, sim->mem->dcache->access_count);
  fprintf(stderr, "TLB: hit: %f%%, [%lu/%lu]\n", (double)sim->mem->tlb->hit_count / sim->mem->tlb->access_count, sim->mem->tlb->hit_count, sim->mem->tlb->access_count);
}
