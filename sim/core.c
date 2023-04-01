#include "core.h"
#include "lsu.h"
#include "memory.h"
#include "csr.h"
#include "riscv.h"
#include <stdio.h>
#include <stdlib.h>

#define CORE_MA_NONE 0
#define CORE_MA_LOAD (CSR_MATCH6_LOAD)
#define CORE_MA_STORE (CSR_MATCH6_STORE)
#define CORE_MA_ACCESS (CSR_MATCH6_LOAD | CSR_MATCH6_STORE)

void core_init(core_t *core, int hart_id, struct memory_t *mem, struct plic_t *plic, struct aclint_t *aclint, struct trigger_t *trigger) {
  // clear gpr
  for (unsigned i = 0; i < NUM_GPR; i++) {
    core->gpr[i] = 0;
  }
  core->window.pc = (unsigned *)calloc(CORE_WINDOW_SIZE, sizeof(unsigned));
  core->window.pc_paddr = (unsigned *)calloc(CORE_WINDOW_SIZE, sizeof(unsigned));
  for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
    core->window.pc[i] = 0xffffffff;
    core->window.pc_paddr[i] = 0xffffffff;
  }
  core->window.inst = (unsigned *)calloc(CORE_WINDOW_SIZE, sizeof(unsigned));
  core->window.exception = (unsigned *)calloc(CORE_WINDOW_SIZE, sizeof(unsigned));
  // init csr
  core->csr = (csr_t *)malloc(sizeof(csr_t));
  csr_init(core->csr);
  // weak reference to csr
  core->mem = mem;
  core->csr->mem = mem;
  core->csr->plic = plic;
  core->csr->aclint = aclint;
  core->csr->trig = trigger;
  core->csr->hart_id = hart_id;
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
    if (src2 == 0) {
      result->rd_data = -1;
    } else {
      result->rd_data = (int)src1 / (int)src2;
    }
    break;
  case 0x5: // DIVU
    if (src2 == 0) {
      result->rd_data = 0xffffffff;
    } else {
      result->rd_data = (unsigned)src1 / (unsigned)src2;
    }
    break;
  case 0x6: // REM
    if (src2 == 0) {
      result->rd_data = src1;
    } else {
      result->rd_data = (int)src1 % (int)src2;
    }
    break;
  case 0x7: // REMU
    if (src2 == 0) {
      result->rd_data = src1;
    } else {
      result->rd_data = (unsigned)src1 % (unsigned)src2;
    }
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
  unsigned pc_paddr = 0;
  for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
    if (core->window.pc[i] == pc) {
      found = 1;
      break;
    }
  }
  if (!found) {
    unsigned window_pc = pc;
    unsigned first_paddr;
    unsigned char *line = NULL;
    exception = memory_address_translation(core->mem, window_pc, &first_paddr, ACCESS_TYPE_INSTRUCTION, prv);
    if (!exception) {
      unsigned index = 0;
      line = (unsigned char *)cache_get_line_ptr(core->mem->icache, (first_paddr & ~(core->mem->icache->line_mask)), CACHE_ACCESS_READ);
      index = first_paddr & core->mem->icache->line_mask;
      // update window
      for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
        core->window.pc[i] = window_pc;
        if (index >= core->mem->icache->line_mask) {
          // get new line
          index = 0;
          exception = memory_address_translation(core->mem, window_pc, &first_paddr, ACCESS_TYPE_INSTRUCTION, prv);
          if (exception == 0) {
            line = (unsigned char *)cache_get_line_ptr(core->mem->icache, (first_paddr & ~(core->mem->icache->line_mask)), CACHE_ACCESS_READ);
          }
        }
        core->window.pc_paddr[i] = first_paddr;
        if ((line[index] & 0x03) == 0x3) {
          if (index + 2 > core->mem->icache->line_mask) {
            core->window.inst[i] = (line[index + 1] << 8) | line[index];
            // get new line
            index = 0;
            unsigned second_paddr;
            exception = memory_address_translation(core->mem, window_pc + 2, &second_paddr, ACCESS_TYPE_INSTRUCTION, prv);
            if (exception == 0) {
              line = (unsigned char *)cache_get_line_ptr(core->mem->icache, (second_paddr & ~(core->mem->icache->line_mask)), CACHE_ACCESS_READ);
            }
            core->window.inst[i] |= ((line[index + 1] << 24) | (line[index] << 16));
            index = 2; // this 2 is ok, not a typo
          } else {
            core->window.inst[i] =
              (line[index + 3] << 24) | (line[index + 2] << 16) |
              (line[index + 1] << 8) | line[index];
            index += 4;
          }
          window_pc += 4;
          first_paddr += 4;
        } else {
          core->window.inst[i] = (line[index + 1] << 8) | line[index];
          index += 2;
          window_pc += 2;
          first_paddr += 2;
        }
        core->window.exception[i] = exception;
      }
    }
  }
  // re-search in window
  for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
    result->inst_window_pc[i] = core->window.pc[i];
    result->inst_window[i] = core->window.inst[i];
    if (core->window.pc[i] == pc) {
      inst = core->window.inst[i];
      w_index = i;
      exception = core->window.exception[i];
      pc_paddr = core->window.pc_paddr[i];
    }
  }
  result->inst = inst;
  result->exception_code = exception;
  result->inst_window_pos = w_index;
  result->pc_paddr = pc_paddr;
  return w_index;
}

// atomic operations
static unsigned op_add(unsigned src1, unsigned src2) { return src1 + src2; }
static unsigned op_swap(unsigned src1, unsigned src2) { return src2; }
static unsigned op_xor(unsigned src1, unsigned src2) { return src1 ^ src2; }
static unsigned op_or(unsigned src1, unsigned src2) { return src1 | src2; }
static unsigned op_and(unsigned src1, unsigned src2) { return src1 & src2; }
static unsigned op_min(unsigned src1, unsigned src2) { return ((int)src1 < (int)src2) ? src1 : src2; }
static unsigned op_max(unsigned src1, unsigned src2) { return ((int)src1 < (int)src2) ? src2 : src1; }
static unsigned op_minu(unsigned src1, unsigned src2) { return (src1 < src2) ? src1 : src2; }
static unsigned op_maxu(unsigned src1, unsigned src2) { return (src1 < src2) ? src2 : src1; }

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
  core_fetch_instruction(core, pc, result, prv);
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
  result->opcode = opcode = riscv_get_opcode(inst);
  // exec
  switch (opcode) {
  case OPCODE_OP_IMM: {
    result->rd_regno = riscv_get_rd(inst);
    result->rs1_regno = riscv_get_rs1(inst);
    unsigned src1 = core->gpr[result->rs1_regno];
    unsigned src2 = riscv_get_immediate(inst);
    unsigned funct = riscv_get_funct3(inst);
    if (funct == 0x0) { // SUBI does not exist
      process_alu(funct, src1, src2, 0, result);
    } else {
      process_alu(funct, src1, src2, (inst & 0x40000000), result);
    }
    break;
  }
  case OPCODE_OP: {
    result->rd_regno = riscv_get_rd(inst);
    result->rs1_regno = riscv_get_rs1(inst);
    result->rs2_regno = riscv_get_rs2(inst);
    unsigned src1 = core->gpr[result->rs1_regno];
    unsigned src2 = core->gpr[result->rs2_regno];
    if (riscv_get_funct7(inst) == 0x01) {
      process_muldiv(riscv_get_funct3(inst), src1, src2, result);
    } else {
      process_alu(riscv_get_funct3(inst), src1, src2, (inst & 0x40000000), result);
    }
    break;
  }
  case OPCODE_AUIPC:
    result->rd_regno = riscv_get_rd(inst);
    result->rd_data = pc + (inst & 0xfffff000);
    break;
  case OPCODE_LUI:
    result->rd_regno = riscv_get_rd(inst);
    result->rd_data = (inst & 0xfffff000);
    break;
  case OPCODE_JALR:
    result->rd_regno = riscv_get_rd(inst);
    result->rd_data = pc_next;
    result->rs1_regno = riscv_get_rs1(inst);
    pc_next = core->gpr[result->rs1_regno] + riscv_get_jalr_offset(inst);
    break;
  case OPCODE_JAL:
    result->rd_regno = riscv_get_rd(inst);
    result->rd_data = pc_next;
    pc_next = pc + riscv_get_jal_offset(inst);
    break;
  case OPCODE_STORE: {
    result->m_access = CORE_MA_STORE;
    result->rs1_regno = riscv_get_rs1(inst);
    result->rs2_regno = riscv_get_rs2(inst);
    result->m_vaddr = core->gpr[result->rs1_regno] + riscv_get_store_offset(inst);
    result->m_data = core->gpr[result->rs2_regno];
    switch (riscv_get_funct3(inst)) {
    case 0x0:
      memory_store(core->mem, 1, result);
      break;
    case 0x1:
      memory_store(core->mem, 2, result);
      break;
    case 0x2:
      memory_store(core->mem, 4, result);
      break;
    default:
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    }
    break;
  }
  case OPCODE_LOAD: {
    result->m_access = CORE_MA_LOAD;
    result->rd_regno = riscv_get_rd(inst);
    result->rs1_regno = riscv_get_rs1(inst);
    result->m_vaddr = core->gpr[result->rs1_regno] + riscv_get_load_offset(inst);
    switch (riscv_get_funct3(inst)) {
    case 0x0: // singed ext byte
      memory_load(core->mem, 1, result);
      result->rd_data = (int)((char)result->rd_data);
      break;
    case 0x1: // signed ext half
      memory_load(core->mem, 2, result);
      result->rd_data = (int)((short)result->rd_data);
      break;
    case 0x2:
      memory_load(core->mem, 4, result);
      break;
    case 0x4:
      memory_load(core->mem, 1, result);
      break;
    case 0x5:
      memory_load(core->mem, 2, result);
      break;
    default:
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    }
    break;
  }
  case OPCODE_AMO: {
    result->m_access = CORE_MA_ACCESS;
    result->rd_regno = riscv_get_rd(inst);
    result->rs1_regno = riscv_get_rs1(inst);
    result->rs2_regno = riscv_get_rs2(inst);
    result->m_vaddr = core->gpr[result->rs1_regno];
    result->m_data = core->gpr[result->rs2_regno];
    switch (riscv_get_funct5(inst)) {
    case 0x002: // Load Reserved
      memory_load_reserved(core->mem, inst & AMO_AQ, result);
      break;
    case 0x003: // Store Conditional
      memory_store_conditional(core->mem, inst & AMO_RL, result);
      break;
    case 0x000: // AMOADD
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_add, result);
      break;
    case 0x001: // AMOSWAP
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_swap, result);
      break;
    case 0x004: // AMOXOR
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_xor, result);
      break;
    case 0x008: // AMOOR
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_or, result);
      break;
    case 0x00c: // AMOAND
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_and, result);
      break;
    case 0x010: // AMOMIN
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_min, result);
      break;
    case 0x014: // AMOMAX
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_max, result);
      break;
    case 0x018: // AMOMINU
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_minu, result);
      break;
    case 0x01c: // AMOMAXU
      memory_atomic_operation(core->mem, inst & AMO_AQ, inst & AMO_RL, op_maxu, result);
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
    if (riscv_get_funct3(inst) & 0x03) {
      result->rd_regno = riscv_get_rd(inst);
    }
    // CSR OPERATIONS
    switch (riscv_get_funct3(inst)) {
    case 0x1: // READ_WRITE
      result->rs1_regno = riscv_get_rs1(inst);
      result->rd_data = csr_csrrw(core->csr, riscv_get_csr_addr(inst), core->gpr[result->rs1_regno], result);
      break;
    case 0x2: // READ_SET
      result->rs1_regno = riscv_get_rs1(inst);
      result->rd_data = csr_csrrs(core->csr, riscv_get_csr_addr(inst), core->gpr[result->rs1_regno], result);
      break;
    case 0x3: // READ_CLEAR
      result->rs1_regno = riscv_get_rs1(inst);
      result->rd_data = csr_csrrc(core->csr, riscv_get_csr_addr(inst), core->gpr[result->rs1_regno], result);
      break;
    case 0x4: // Hypervisor Extension
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    case 0x5: // READ_WRITE (imm)
      result->rs1_regno = 0;
      result->rd_data = csr_csrrw(core->csr, riscv_get_csr_addr(inst), riscv_get_csr_imm(inst), result);
      break;
    case 0x6: // READ_SET (imm)
      result->rs1_regno = 0;
      result->rd_data = csr_csrrs(core->csr, riscv_get_csr_addr(inst), riscv_get_csr_imm(inst), result);
      break;
    case 0x7: // READ_CLEAR (imm)
      result->rs1_regno = 0;
      result->rd_data = csr_csrrc(core->csr, riscv_get_csr_addr(inst), riscv_get_csr_imm(inst), result);
      break;
    default: // OTHER SYSTEM OPERATIONS (ECALL, EBREAK, MRET, etc.)
      switch (riscv_get_funct12(inst)) {
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
        result->flush = 1;
        break;
      case 0x105: // WFI
        break;
      default:
        if (riscv_get_funct7(inst) == 0x09) {
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
    result->rs1_regno = riscv_get_rs1(inst);
    result->rs2_regno = riscv_get_rs2(inst);
    unsigned src1 = core->gpr[result->rs1_regno];
    unsigned src2 = core->gpr[result->rs2_regno];
    unsigned pred = 0;
    switch (riscv_get_funct3(inst)) {
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
      pc_next = pc + riscv_get_branch_offset(inst);
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
  }
  return;
}

void core_fini(core_t *core) {
  free(core->window.pc);
  free(core->window.inst);
  free(core->window.exception);
  csr_fini(core->csr);
  free(core->csr);
  return;
}
