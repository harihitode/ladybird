#include "sim.h"
#include "elfloader.h"
#include "memory.h"
#include "csr.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_GPR 32
#define NUM_FPR 32
// csr address

void sim_init(sim_t *sim, const char *elf_path) {
  // clear gpr
  sim->gpr = (unsigned *)calloc(NUM_GPR, sizeof(unsigned));
  // init memory
  sim->mem = (memory_t *)malloc(sizeof(memory_t));
  memory_init(sim->mem);
  // init csr
  sim->csr = (csr_t *)malloc(sizeof(csr_t));
  csr_init(sim->csr);
  sim->csr->sim = sim;
  // init elf loader
  sim->elf = (elf_t *)malloc(sizeof(elf_t));
  elf_init(sim->elf, elf_path);
  // program load to memory
  for (unsigned i = 0; i < sim->elf->programs; i++) {
    for (unsigned j = 0; j < sim->elf->program_size[i]; j++) {
      memory_store(sim->mem, sim->elf->program_base[i] + j, sim->elf->program[i][j]);
    }
  }
  // set entry program counter
  sim->pc = sim->elf->entry_address;
  return;
}

void sim_trap(sim_t *sim, void (*callback)(unsigned, sim_t *)) {
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
static unsigned get_immediate(unsigned inst) {
  switch (get_opcode(inst)) {
  case OPCODE_LOAD:
  case OPCODE_JALR:
    return (int)inst >> 20;
  case OPCODE_OP_IMM:
    switch (get_funct3(inst)) {
    case 0x2:
    case 0x5:
      return ((int)inst >> 20) & 0x0000001f;
    default:
      return (int)inst >> 20;
    }
  case OPCODE_STORE:
    return (((int)inst >> 25) << 5) | ((inst >> 7) & 0x0000001f);
  case OPCODE_AUIPC:
  case OPCODE_LUI:
    return inst & 0xfffff000;
  case OPCODE_BRANCH:
    return (((int)inst >> 19) & 0xfffff000) |
      (((inst >> 25) << 5) & 0x000007e0) |
      (((inst >> 7) & 0x00000001) << 11) | ((inst >> 7) & 0x0000001e);
  case OPCODE_JAL:
    return (((int)inst >> 11) & 0xfff00000) | (inst & 0x000ff000) |
      (((inst >> 20) & 0x00000001) << 11) |
      ((inst >> 20) & 0x000007fe);
  case OPCODE_SYSTEM:
    return (inst >> 20) & 0x00000fff;
  default:
    return 0;
  }
}

void sim_step(sim_t *sim) {
  // fetch
  unsigned inst = sim_read_memory(sim, sim->pc);
  unsigned opcode;
  unsigned rs1, rs2, rd;
  unsigned src1, src2, immediate;
  unsigned result = 0;
  unsigned f7 = get_funct7(inst);
  unsigned f5 = get_funct5(inst);
  unsigned f3 = get_funct3(inst);
  opcode = get_opcode(inst);
  rs1 = get_rs1(inst);
  rs2 = get_rs2(inst);
  rd = get_rd(inst);
  immediate = get_immediate(inst);
  // read
  src1 = sim_read_register(sim, rs1);
  src2 = sim_read_register(sim, rs2);
  // exec
  switch (opcode) {
  case OPCODE_OP_IMM:
    src2 = immediate; // ALU with src2 as immediate
    // fall-through
  case OPCODE_OP:
    if (opcode == OPCODE_OP && f7 == 0x00000001) {
      // MUL/DIV
      switch (f3) {
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
      switch (f3) {
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
    result = sim->pc + immediate;
    break;
  case OPCODE_LUI:
    result = immediate;
    break;
  case OPCODE_JALR:
  case OPCODE_JAL:
    // for link address
    result = sim->pc + 4;
    break;
  case OPCODE_STORE:
    {
      unsigned addr = src1 + immediate;
      char b0 = (src2 >>  0) & 0x000000ff;
      char b1 = (src2 >>  8) & 0x000000ff;
      char b2 = (src2 >> 16) & 0x000000ff;
      char b3 = (src2 >> 24) & 0x000000ff;
      switch (f3) {
      case 0x2:
        memory_store(sim->mem, addr + 3, b3);
        memory_store(sim->mem, addr + 2, b2);
        // fall through
      case 0x1:
        memory_store(sim->mem, addr + 1, b1);
        // fall through
      case 0x0:
        memory_store(sim->mem, addr + 0, b0);
        break;
      default:
        break;
      }
    }
    break;
  case OPCODE_LOAD:
    {
      unsigned addr = src1 + immediate;
      char b0 = memory_load(sim->mem, addr + 0);
      char b1 = memory_load(sim->mem, addr + 1);
      char b2 = memory_load(sim->mem, addr + 2);
      char b3 = memory_load(sim->mem, addr + 3);
      switch (f3) {
      case 0x0:
        result = (int)b0;
        break;
      case 0x1:
        result = (((int)b1 << 8) & 0xffffff00) | (b0 & 0x000000ff);
        break;
      case 0x2:
        result =
          (((unsigned)b3 << 24) & 0xff000000) | (((unsigned)b2 << 16) & 0x00ff0000) |
          (((unsigned)b1 <<  8) & 0x0000ff00) | (b0 & 0x000000ff);
        break;
      case 0x4:
        result = (unsigned)b0;
        break;
      case 0x5:
        result = (((unsigned)b1 << 8) & 0x0000ff00) | (b0 & 0x000000ff);
        break;
      default:
        csr_trap(sim->csr, TRAP_CODE_INVALID_INSTRUCTION);
        break;
      }
    }
    break;
  case OPCODE_AMO:
    if (f5 == 0x002) {
      result = memory_load_reserved(sim->mem, src1);
    } else if (f5 == 0x003) {
      result = memory_store_conditional(sim->mem, src1, src2);
    } else {
      result = sim_read_memory(sim, src1);
    }
    switch (f5) {
    case 0x000: // AMOADD
      sim_write_memory(sim, src1, result + src2);
      break;
    case 0x001: // AMOSWAP
      sim_write_memory(sim, src1, src2);
      break;
    case 0x002: // Load Reserved
      break;
    case 0x003: // Store Conditional
      break;
    case 0x004: // AMOXOR
      sim_write_memory(sim, src1, result ^ src2);
      break;
    case 0x008: // AMOOR
      sim_write_memory(sim, src1, result | src2);
      break;
    case 0x00c: // AMOAND
      sim_write_memory(sim, src1, result & src2);
      break;
    case 0x010: // AMOMIN
      sim_write_memory(sim, src1, ((int)result < (int)src2) ? result : src2);
      break;
    case 0x014: // AMOMAX
      sim_write_memory(sim, src1, ((int)result < (int)src2) ? src2 : result);
      break;
    case 0x018: // AMOMINU
      sim_write_memory(sim, src1, (result < src2) ? result : src2);
      break;
    case 0x01c: // AMOMAXU
      sim_write_memory(sim, src1, (result < src2) ? src2 : result);
      break;
    default:
      csr_trap(sim->csr, TRAP_CODE_INVALID_INSTRUCTION);
      break;
    }
    break;
  case OPCODE_MISC_MEM:
    // TODO: currently no implementataion
    break;
  case OPCODE_SYSTEM:
    if (f3 != 0) {
      // CSR OPERATIONS
      unsigned csr_addr = immediate;
      unsigned csr_write_value = (f3 > 3) ? rs1 : src1;
      switch (f3 & 0x03) {
      case 0x1: // READ_WRITE
        result = csr_csrrw(sim->csr, csr_addr, csr_write_value);
        break;
      case 0x2: // READ_SET
        result = csr_csrrs(sim->csr, csr_addr, csr_write_value);
        break;
      case 0x3: // READ_CLEAR
        result = csr_csrrc(sim->csr, csr_addr, csr_write_value);
        break;
      default:
        break;
      }
    }
    break;
  case OPCODE_BRANCH:
    break;
  default:
    // invalid opcode
    csr_trap(sim->csr, TRAP_CODE_INVALID_INSTRUCTION);
    break;
  }
  // writeback
  switch (opcode) {
  case OPCODE_OP_IMM:
  case OPCODE_OP:
  case OPCODE_AUIPC:
  case OPCODE_LUI:
  case OPCODE_JAL:
  case OPCODE_JALR:
  case OPCODE_LOAD:
  case OPCODE_AMO:
    sim_write_register(sim, rd, result);
    break;
  case OPCODE_SYSTEM:
    if (f3 != 0) {
      sim_write_register(sim, rd, result);
    }
    break;
  default:
    break;
  }
  // pc update
  switch (opcode) {
  case OPCODE_BRANCH:
    switch (f3) {
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
    }
    if (result) {
      sim->pc = sim->pc + immediate;
    } else {
      sim->pc = sim->pc + 4;
    }
    break;
  case OPCODE_JALR:
    sim->pc = src1 + immediate;
    break;
  case OPCODE_JAL:
    sim->pc = sim->pc + immediate;
    break;
  case OPCODE_SYSTEM:
    if (f3 == 0) {
      // SYSTEM OPERATIONS (ECALL, EBREAK, MRET, etc.)
      switch (f7) {
      case 0x00:
        if (rs2 == 0) {
          csr_trap(sim->csr, TRAP_CODE_ECALL);
        } else if (rs1 == 1) {
          csr_trap(sim->csr, TRAP_CODE_EBREAK);
          sim->pc = sim->pc + 4;
        } else {
          csr_trap(sim->csr, TRAP_CODE_INVALID_INSTRUCTION);
        }
        break;
      case 0x18:
        if (rs2 == 2) {
          // MRET
          sim->pc = csr_csrr(sim->csr, CSR_ADDR_M_EPC);
        } else {
          csr_trap(sim->csr, TRAP_CODE_INVALID_INSTRUCTION);
        }
      case 0x09:
        // SFENCE.VMA
        // TODO: clear TLA
        sim->pc = sim->pc + 4;
        break;
      default:
        csr_trap(sim->csr, TRAP_CODE_INVALID_INSTRUCTION);
        break;
      }
    } else {
      sim->pc = sim->pc + 4;
    }
    break;
  default:
    sim->pc = sim->pc + 4;
    break;
  }
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
  unsigned ret;
  char b0, b1, b2, b3;
  b0 = memory_load(sim->mem, addr + 0);
  b1 = memory_load(sim->mem, addr + 1);
  b2 = memory_load(sim->mem, addr + 2);
  b3 = memory_load(sim->mem, addr + 3);
  ret =
    ((b3 << 24) & 0xff000000) | ((b2 << 16) & 0x00ff0000) |
    ((b1 <<  8) & 0x0000ff00) | ((b0 <<  0) & 0x000000ff);
  return ret;
}

void sim_write_memory(sim_t *sim, unsigned addr, unsigned value) {
  memory_store(sim->mem, addr + 0, (char)((value >>  0) & 0x000000ff));
  memory_store(sim->mem, addr + 1, (char)((value >>  8) & 0x000000ff));
  memory_store(sim->mem, addr + 2, (char)((value >> 16) & 0x000000ff));
  memory_store(sim->mem, addr + 3, (char)((value >> 24) & 0x000000ff));
  return;
}

void sim_fini(sim_t *sim) {
  free(sim->gpr);
  memory_fini(sim->mem);
  free(sim->mem);
  csr_fini(sim->csr);
  free(sim->csr);
  elf_fini(sim->elf);
  free(sim->elf);
  return;
}
