#include "core.h"
#include "memory.h"
#include "csr.h"
#include "riscv.h"
#include <stdio.h>
#include <stdlib.h>

#define CORE_MA_NONE 0
#define CORE_MA_LOAD (CSR_MATCH6_LOAD)
#define CORE_MA_STORE (CSR_MATCH6_STORE)
#define CORE_MA_ACCESS (CSR_MATCH6_LOAD | CSR_MATCH6_STORE)

void core_init(core_t *core) {
  // clear gpr
  for (unsigned i = 0; i < NUM_GPR; i++) {
    core->gpr[i] = 0;
  }
  core->window.pc = (unsigned *)calloc(CORE_WINDOW_SIZE, sizeof(unsigned));
  for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
    core->window.pc[i] = 0xffffffff;
  }
  core->window.inst = (unsigned *)calloc(CORE_WINDOW_SIZE, sizeof(unsigned));
  core->window.exception = (unsigned *)calloc(CORE_WINDOW_SIZE, sizeof(unsigned));
}

static unsigned get_opcode(unsigned inst) { return inst & 0x0000007f; }
static unsigned get_rs1(unsigned inst) { return (inst >> 15) & 0x0000001f; }
static unsigned get_rs2(unsigned inst) { return (inst >> 20) & 0x0000001f; }
static unsigned get_rd(unsigned inst) { return (inst >> 7) & 0x0000001f; }
static unsigned get_funct3(unsigned inst) { return (inst >> 12) & 0x00000007; }
static unsigned get_funct5(unsigned inst) { return (inst >> 27) & 0x0000001f; }
static unsigned get_funct7(unsigned inst) { return (inst >> 25) & 0x0000007f; }
static unsigned get_funct12(unsigned inst) { return (inst >> 20) & 0x00000fff; }
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
static unsigned get_csr_imm(unsigned inst) { return (inst >> 15) & 0x0000001f; }
static unsigned get_immediate(unsigned inst) {
  return ((int)inst >> 20);
}

static void process_alu(unsigned funct, unsigned src1, unsigned src2, unsigned alt, struct core_step_result *result) {
  switch (funct) {
  case 0x0: // ADD, SUB, ADDI
    if (alt) {
      result->rd_data = src1 - src2; // SUB
    } else {
      result->rd_data = src1 + src2; // ADD, ADDI
    }
    break;
  case 0x1: // SLL
    result->rd_data = (src1 << (src2 & 0x0000001F));
    break;
  case 0x2: // Set Less-Than
    result->rd_data = ((int)src1 < (int)src2) ? 1 : 0;
    break;
  case 0x3: // Set Less-Than Unsigned
    result->rd_data = (src1 < src2) ? 1 : 0;
    break;
  case 0x4: // Logical XOR
    result->rd_data = src1 ^ src2;
    break;
  case 0x5: // SRA, SRL
    if (alt) {
      result->rd_data = (int)src1 >> (src2 & 0x0000001F);
    } else {
      result->rd_data = src1 >> (src2 & 0x0000001F);
    }
    break;
  case 0x6: // Logical OR
    result->rd_data = src1 | src2;
    break;
  case 0x7: // Logical AND
    result->rd_data = src1 & src2;
    break;
  }
}

void process_muldiv(unsigned funct, unsigned src1, unsigned src2, struct core_step_result *result) {
  switch (funct) {
  case 0x0:
    result->rd_data = ((long long)src1 * (long long)src2) & 0xffffffff;
    break;
  case 0x1: // MULH (extended: signed * signedb)
    result->rd_data = ((long long)src1 * (long long)src2) >> 32;
    break;
  case 0x2: // MULHSU (extended: signed * unsigned)
    result->rd_data = ((long long)src1 * (unsigned long long)src2) >> 32;
    break;
  case 0x3: // MULHU (extended: unsigned * unsigned)
    result->rd_data = ((unsigned long long)src1 * (unsigned long long)src2) >> 32;
    break;
  case 0x4: // DIV
    result->rd_data = (int)src1 / (int)src2;
    break;
  case 0x5: // DIVU
    result->rd_data = (unsigned)src1 / (unsigned)src2;
    break;
  case 0x6: // REM
    result->rd_data = (int)src1 % (int)src2;
    break;
  case 0x7: // REMU
    result->rd_data = (unsigned)src1 % (unsigned)src2;
    break;
  }
}

void core_window_flush(core_t *core) {
  for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
    core->window.pc[i] = 0xffffffff;
  }
}

static unsigned core_fetch_instruction(core_t *core, unsigned pc, struct core_step_result *result, unsigned prv) {
  // hit the instruction fetch window
  int found = 0;
  unsigned inst = 0;
  unsigned exception = 0;
  unsigned w_index = 0;
  for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
    if (core->window.pc[i] == pc) {
      found = 1;
      inst = core->window.inst[i];
      exception = core->window.exception[i];
      w_index = i;
      break;
    }
  }
  if (!found) {
    unsigned window_pc = pc;
    unsigned paddr;
    unsigned char *line = NULL;
    unsigned index = 0;
    exception = memory_address_translation(core->mem, window_pc, &paddr, ACCESS_TYPE_INSTRUCTION, prv);
    if (!exception) {
      line = (unsigned char *)cache_get(core->mem->icache, (paddr & ~(core->mem->icache->line_mask)), CACHE_READ);
      index = paddr & core->mem->icache->line_mask;
      // update window
      for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
        core->window.pc[i] = window_pc;
        if (index >= core->mem->icache->line_mask) {
          // get new line
          index = 0;
          exception = memory_address_translation(core->mem, window_pc, &paddr, ACCESS_TYPE_INSTRUCTION, prv);
          line = (unsigned char *)cache_get(core->mem->icache, (paddr & ~(core->mem->icache->line_mask)), CACHE_READ);
        }
        if ((line[index] & 0x03) == 0x3) {
          if (index + 2 > core->mem->icache->line_mask) {
            core->window.inst[i] = (line[index + 1] << 8) | line[index];
            // get new line
            index = 0;
            exception = memory_address_translation(core->mem, window_pc + 2, &paddr, ACCESS_TYPE_INSTRUCTION, prv);
            line = (unsigned char *)cache_get(core->mem->icache, (paddr & ~(core->mem->icache->line_mask)), CACHE_READ);
            core->window.inst[i] |= ((line[index + 1] << 24) | (line[index] << 16));
            index = 2; // this 2 is ok, not a typo
          } else {
            core->window.inst[i] =
              (line[index + 3] << 24) | (line[index + 2] << 16) |
              (line[index + 1] << 8) | line[index];
            index += 4;
          }
          window_pc += 4;
        } else {
          core->window.inst[i] = (line[index + 1] << 8) | line[index];
          index += 2;
          window_pc += 2;
        }
        core->window.exception[i] = exception;
      }
      // re-search in window
      for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
        if (core->window.pc[i] == pc) {
          inst = core->window.inst[i];
          w_index = i;
          exception = core->window.exception[i];
          break;
        }
      }
    }
  }
  result->inst = inst;
  result->exception_code = exception;
  return w_index;
}

void core_step(core_t *core, unsigned pc, struct core_step_result *result, unsigned prv) {
  // init result
  result->pc = pc;
  result->cycle = core->csr->cycle;
  result->prv = prv;
  result->pc_next = pc;
  result->flush = 0;
  // instruction fetch & decode
  unsigned inst;
  unsigned pc_next;
  unsigned opcode;
#ifdef REGISTER_ACCESS_STATS
  int window_index = core_fetch_instruction(core, pc, result, prv);
#else
  core_fetch_instruction(core, pc, result, prv);
#endif
  if (result->exception_code != 0) {
    return;
  }
  if ((result->inst & 0x03) == 0x03) {
    pc_next = pc + 4;
    inst = result->inst;
  } else {
    pc_next = pc + 2;
    inst = riscv_decompress(result->inst);
  }
  opcode = get_opcode(inst);
  // exec
  switch (opcode) {
  case OPCODE_OP_IMM: {
    result->rd_regno = get_rd(inst);
    result->rs1_regno = get_rs1(inst);
    unsigned src1 = core->gpr[result->rs1_regno];
    unsigned src2 = get_immediate(inst);
    unsigned funct = get_funct3(inst);
    if (funct == 0x0) { // SUBI does not exist
      process_alu(funct, src1, src2, 0, result);
    } else {
      process_alu(funct, src1, src2, (inst & 0x40000000), result);
    }
    break;
  }
  case OPCODE_OP: {
    result->rd_regno = get_rd(inst);
    result->rs1_regno = get_rs1(inst);
    result->rs2_regno = get_rs2(inst);
    unsigned src1 = core->gpr[result->rs1_regno];
    unsigned src2 = core->gpr[result->rs2_regno];
    if (get_funct7(inst) == 0x01) {
      process_muldiv(get_funct3(inst), src1, src2, result);
    } else {
      process_alu(get_funct3(inst), src1, src2, (inst & 0x40000000), result);
    }
    break;
  }
  case OPCODE_AUIPC:
    result->rd_regno = get_rd(inst);
    result->rd_data = pc + (inst & 0xfffff000);
    break;
  case OPCODE_LUI:
    result->rd_regno = get_rd(inst);
    result->rd_data = (inst & 0xfffff000);
    break;
  case OPCODE_JALR:
    result->rd_regno = get_rd(inst);
    result->rd_data = pc_next;
    result->rs1_regno = get_rs1(inst);
    pc_next = core->gpr[result->rs1_regno] + get_jalr_offset(inst);
    break;
  case OPCODE_JAL:
    result->rd_regno = get_rd(inst);
    result->rd_data = pc_next;
    pc_next = pc + get_jal_offset(inst);
    break;
  case OPCODE_STORE: {
    result->m_access = CORE_MA_STORE;
    result->rs1_regno = get_rs1(inst);
    result->rs2_regno = get_rs2(inst);
    result->m_vaddr = core->gpr[result->rs1_regno] + get_store_offset(inst);
    result->m_data = core->gpr[result->rs2_regno];
    switch (get_funct3(inst)) {
    case 0x0:
      result->exception_code = memory_store(core->mem, result->m_vaddr, result->m_data, 1, prv);
      break;
    case 0x1:
      result->exception_code = memory_store(core->mem, result->m_vaddr, result->m_data, 2, prv);
      break;
    case 0x2:
      result->exception_code = memory_store(core->mem, result->m_vaddr, result->m_data, 4, prv);
      break;
    default:
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    }
    break;
  }
  case OPCODE_LOAD: {
    result->m_access = CORE_MA_LOAD;
    result->rd_regno = get_rd(inst);
    result->rs1_regno = get_rs1(inst);
    result->m_vaddr = core->gpr[result->rs1_regno] + get_load_offset(inst);
    switch (get_funct3(inst)) {
    case 0x0: // singed ext byte
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 1, prv);
      result->rd_data = (int)((char)result->rd_data);
      break;
    case 0x1: // signed ext half
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 2, prv);
      result->rd_data = (int)((short)result->rd_data);
      break;
    case 0x2:
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      break;
    case 0x4:
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 1, prv);
      break;
    case 0x5:
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 2, prv);
      break;
    default:
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    }
    break;
  }
  case OPCODE_AMO: {
    result->m_access = CORE_MA_ACCESS;
    result->rd_regno = get_rd(inst);
    result->rs1_regno = get_rs1(inst);
    result->rs2_regno = get_rs2(inst);
    result->m_vaddr = core->gpr[result->rs1_regno];
    unsigned src2 = core->gpr[result->rs2_regno];
    switch (get_funct5(inst)) {
    case 0x002: // Load Reserved
      result->exception_code = memory_load_reserved(core->mem, result->m_vaddr, &result->rd_data, prv);
      break;
    case 0x003: // Store Conditional
      result->exception_code = memory_store_conditional(core->mem, result->m_vaddr, src2, &result->rd_data, prv);
      break;
    case 0x000: // AMOADD
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr, result->rd_data + src2, 4, prv);
      break;
    case 0x001: // AMOSWAP
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr, src2, 4, prv);
      break;
    case 0x004: // AMOXOR
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr, result->rd_data ^ src2, 4, prv);
      break;
    case 0x008: // AMOOR
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr, result->rd_data | src2, 4, prv);
      break;
    case 0x00c: // AMOAND
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr, result->rd_data & src2, 4, prv);
      break;
    case 0x010: // AMOMIN
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr,
                                            ((int)result->rd_data < (int)src2) ? result->rd_data : src2, 4, prv);
      break;
    case 0x014: // AMOMAX
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr,
                                            ((int)result->rd_data > (int)src2) ? result->rd_data : src2, 4, prv);
      break;
    case 0x018: // AMOMINU
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr,
                                            (result->rd_data < src2) ? result->rd_data : src2, 4, prv);
      break;
    case 0x01c: // AMOMAXU
      result->exception_code = memory_load(core->mem, result->m_vaddr, &result->rd_data, 4, prv);
      result->exception_code = memory_store(core->mem, result->m_vaddr,
                                            (result->rd_data > src2) ? result->rd_data : src2, 4, prv);
      break;
    default:
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    }
    break;
  }
  case OPCODE_MISC_MEM:
    // any fence means cache flush for this system
    memory_dcache_write_back(core->mem);
    break;
  case OPCODE_SYSTEM:
    if (get_funct3(inst) & 0x03) {
      result->rd_regno = get_rd(inst);
    }
    // CSR OPERATIONS
    switch (get_funct3(inst)) {
    case 0x1: // READ_WRITE
      result->rs1_regno = get_rs1(inst);
      result->rd_data = csr_csrrw(core->csr, get_csr_addr(inst), core->gpr[result->rs1_regno], result);
      break;
    case 0x2: // READ_SET
      result->rs1_regno = get_rs1(inst);
      result->rd_data = csr_csrrs(core->csr, get_csr_addr(inst), core->gpr[result->rs1_regno], result);
      break;
    case 0x3: // READ_CLEAR
      result->rs1_regno = get_rs1(inst);
      result->rd_data = csr_csrrc(core->csr, get_csr_addr(inst), core->gpr[result->rs1_regno], result);
      break;
    case 0x4: // Hypervisor Extension
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    case 0x5: // READ_WRITE (imm)
      result->rd_data = csr_csrrw(core->csr, get_csr_addr(inst), get_csr_imm(inst), result);
      break;
    case 0x6: // READ_SET (imm)
      result->rd_data = csr_csrrs(core->csr, get_csr_addr(inst), get_csr_imm(inst), result);
      break;
    case 0x7: // READ_CLEAR (imm)
      result->rd_data = csr_csrrc(core->csr, get_csr_addr(inst), get_csr_imm(inst), result);
      break;
    default: // OTHER SYSTEM OPERATIONS (ECALL, EBREAK, MRET, etc.)
      switch (get_funct12(inst)) {
      case 0x000: // ECALL
        if (core->csr->mode == PRIVILEGE_MODE_M) {
          result->exception_code = TRAP_CODE_ENVIRONMENT_CALL_M;
        } else if (core->csr->mode == PRIVILEGE_MODE_S) {
          result->exception_code = TRAP_CODE_ENVIRONMENT_CALL_S;
        } else {
          result->exception_code = TRAP_CODE_ENVIRONMENT_CALL_U;
        }
        break;
      case 0x001: // EBREAK
        result->exception_code = TRAP_CODE_BREAKPOINT;
        break;
      case 0x010: // URET
      case 0x102: // SRET
      case 0x302: // MRET
        result->trapret = 1;
        break;
      case 0x105: // WFI
        break;
      default:
        if (get_funct7(inst) == 0x09) {
          // SFENCE.VMA
          memory_icache_invalidate(core->mem);
          memory_dcache_write_back(core->mem);
          memory_tlb_clear(core->mem);
        } else {
          result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
        }
        break;
      }
    }
    break;
  case OPCODE_BRANCH: {
    // read
    result->rs1_regno = get_rs1(inst);
    result->rs2_regno = get_rs2(inst);
    unsigned src1 = core->gpr[result->rs1_regno];
    unsigned src2 = core->gpr[result->rs2_regno];
    unsigned pred = 0;
    switch (get_funct3(inst)) {
    case 0x0:
      pred = (src1 == src2) ? 1 : 0;
      break;
    case 0x1:
      pred = (src1 != src2) ? 1 : 0;
      break;
    case 0x4:
      pred = ((int)src1 < (int)src2) ? 1 : 0;
      break;
    case 0x5:
      pred = ((int)src1 >= (int)src2) ? 1 : 0;
      break;
    case 0x6:
      pred = (src1 < src2) ? 1 : 0;
      break;
    case 0x7:
      pred = (src1 >= src2) ? 1 : 0;
      break;
    default:
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    }
    if (pred == 1) {
      pc_next = pc + get_branch_offset(inst);
    }
    break;
  }
  default:
    // invalid opcode
    result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
    break;
  }
  if (result->exception_code != 0) {
    return;
  } else if (result->rd_regno != 0) {
    core->gpr[result->rd_regno] = result->rd_data;
  }
  result->pc_next = pc_next;
  if (result->flush) {
    core_window_flush(core);
    memory_tlb_clear(core->mem);
    memory_icache_invalidate(core->mem);
    memory_dcache_invalidate(core->mem);
  }

#ifdef REGISTER_ACCESS_STATS
  result->rs1_read_skip = 0;
  result->rs2_read_skip = 0;
  result->rd_write_skip = 0;

  if (opcode == OPCODE_OP || opcode == OPCODE_OP_IMM) {
    unsigned r_break = 0;
    unsigned w_break = 0;
    for (int i = 1; i < 4; i++) {
      // read skip search
      int r_index = window_index - i;
      if (!r_break && r_index < CORE_WINDOW_SIZE && core->window.pc[r_index] != 0xffffffff) {
        unsigned r_inst = riscv_decompress(core->window.inst[r_index]);
        unsigned r_opcode = get_opcode(r_inst);
        if (r_opcode == OPCODE_OP || r_opcode == OPCODE_OP_IMM ||
            r_opcode == OPCODE_LUI || r_opcode == OPCODE_AUIPC) {
          if (result->rs1_regno != 0 && result->rs1_regno == get_rd(r_inst)) {
            result->rs1_read_skip = 1;
          }
          if (opcode == OPCODE_OP && result->rs2_regno != 0 && result->rs2_regno == get_rd(r_inst)) {
            result->rs2_read_skip = 1;
          }
        } else if (r_opcode == OPCODE_BRANCH || r_opcode == OPCODE_JAL || r_opcode == OPCODE_JALR) {
          r_break = 1;
        }
      }
      // write skip search
      int w_index = window_index + i;
      if (!w_break && w_index >= 0 && core->window.pc[w_index] != 0xffffffff) {
        unsigned w_inst = riscv_decompress(core->window.inst[w_index]);
        unsigned w_opcode = get_opcode(w_inst);
        if (w_opcode == OPCODE_OP || w_opcode == OPCODE_OP_IMM ||
            w_opcode == OPCODE_LUI || w_opcode == OPCODE_AUIPC || w_opcode == OPCODE_LOAD) {
          if (opcode == OPCODE_OP && result->rd_regno != 0 && result->rd_regno == get_rd(w_inst)) {
            result->rd_write_skip = 1;
          }
        } else if (w_opcode == OPCODE_BRANCH || w_opcode == OPCODE_JAL || w_opcode == OPCODE_JALR) {
          w_break = 1;
        }
      }
    }
  }
#endif
  return;
}

void core_fini(core_t *core) {
  free(core->window.pc);
  free(core->window.inst);
  free(core->window.exception);
  return;
}
