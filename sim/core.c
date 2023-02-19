#include "core.h"
#include "memory.h"
#include "csr.h"
#include <stdio.h>
#include <stdlib.h>

#define CORE_MA_NONE 0
#define CORE_MA_LOAD (CSR_MATCH6_LOAD)
#define CORE_MA_STORE (CSR_MATCH6_STORE)
#define CORE_MA_ACCESS (CSR_MATCH6_LOAD | CSR_MATCH6_STORE)
#define CORE_WINDOW_SIZE 8

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

static unsigned inst_addi(unsigned rd, unsigned rs1, unsigned imm) {
  return (imm << 20) | (rs1 << 15) | (0b000 << 12) | (rd << 7) | OPCODE_OP_IMM;
}

static unsigned inst_slli(unsigned rd, unsigned rs1, unsigned shamt) {
  return ((shamt & 0x3f) << 20) | (rs1 << 15) | (0b001 << 12) | (rd << 7) | OPCODE_OP_IMM;
}

static unsigned inst_lw(unsigned rd, unsigned base, unsigned offs) {
  return (offs << 20) | (base << 15) | (0b010 << 12) | (rd << 7) | OPCODE_LOAD;
}

static unsigned inst_sw(unsigned base, unsigned src, unsigned offs) {
  return ((offs & 0x0fe0) << 20) | (src << 20) | (base << 15) | (0b010 << 12) |
    ((offs & 0x01f) << 7) | OPCODE_STORE;
}

static unsigned inst_ebreak() {
  return 0x00100073;
}

static unsigned inst_add(unsigned rd, unsigned rs1, unsigned rs2) {
  return (rs2 << 20) | (rs1 << 15) | (0b000 << 12) | (rd << 7) | OPCODE_OP;
}

static unsigned inst_jalr(unsigned rd, unsigned rs1, unsigned offs) {
  return (offs << 20) | (rs1 << 15) | (rd << 7) | OPCODE_JALR;
}

static unsigned inst_jal(unsigned rd, unsigned offs) {
  return ((offs & 0x00100000) << 11) | ((offs & 0x07fe) << 20) | ((offs & 0x0800) << 9) | ((offs & 0x000ff000)) | (rd << 7) | OPCODE_JAL;
}

static unsigned inst_lui(unsigned rd, unsigned imm) {
  return (imm & 0xfffff000) | (rd << 7) | OPCODE_LUI;
}

static unsigned inst_beq(unsigned rs1, unsigned rs2, unsigned offs) {
  return ((offs & 0x1000) << 19) | ((offs & 0x07e0) << 20) | (rs2 << 20) | (rs1 << 15) | (0b000 << 12) | ((offs & 0x01e) << 7) | ((offs & 0x0800) >> 4) | OPCODE_BRANCH;
}

static unsigned inst_bne(unsigned rs1, unsigned rs2, unsigned offs) {
  return ((offs & 0x1000) << 19) | ((offs & 0x07e0) << 20) | (rs2 << 20) | (rs1 << 15) | (0b001 << 12) | ((offs & 0x01e) << 7) | ((offs & 0x0800) >> 4) | OPCODE_BRANCH;
}

static unsigned inst_sub(unsigned rd, unsigned rs1, unsigned rs2) {
  return (rs2 << 20) | (rs1 << 15) | (0b000 << 12) | (rd << 7) | OPCODE_OP | 0x40000000;
}

static unsigned inst_xor(unsigned rd, unsigned rs1, unsigned rs2) {
  return (rs2 << 20) | (rs1 << 15) | (0b100 << 12) | (rd << 7) | OPCODE_OP;
}

static unsigned inst_or(unsigned rd, unsigned rs1, unsigned rs2) {
  return (rs2 << 20) | (rs1 << 15) | (0b110 << 12) | (rd << 7) | OPCODE_OP;
}

static unsigned inst_and(unsigned rd, unsigned rs1, unsigned rs2) {
  return (rs2 << 20) | (rs1 << 15) | (0b111 << 12) | (rd << 7) | OPCODE_OP;
}

static unsigned inst_andi(unsigned rd, unsigned rs1, unsigned imm) {
  return (imm << 20) | (rs1 << 15) | (0b111 << 12) | (rd << 7) | OPCODE_OP_IMM;
}

static unsigned inst_srli(unsigned rd, unsigned rs1, unsigned shamt) {
  return ((shamt & 0x3f) << 20) | (rs1 << 15) | (0b101 << 12) | (rd << 7) | OPCODE_OP_IMM;
}

static unsigned inst_srai(unsigned rd, unsigned rs1, unsigned shamt) {
  return ((shamt & 0x3f) << 20) | (rs1 << 15) | (0b101 << 12) | (rd << 7) | OPCODE_OP_IMM | 0x40000000;
}

static unsigned decompress(unsigned inst) {
  unsigned ret = 0; // illegal
  switch (inst & 0x03) {
  case 0x00: {
    unsigned rs1 = ((inst >> 7) & 0x00000007) | 0x00000008;
    unsigned rs2 = ((inst >> 2) & 0x00000007) | 0x00000008;
    unsigned rd = rs2;
    // quadrant 0
    switch ((inst >> 13) & 0x7) {
    case 0b000: { // ADDI4SPN
      unsigned imm = ((((inst >> 5) & 0x00000001) << 3) |
                      (((inst >> 6) & 0x00000001) << 2) |
                      (((inst >> 7) & 0x0000000f) << 6) |
                      (((inst >> 11) & 0x00000003) << 4));
      ret = inst_addi(rd, REG_SP, imm);
      break;
    }
    case 0b010: { // LW
      unsigned offs = ((((inst >> 5) & 0x00000001) << 6) |
                       (((inst >> 6) & 0x00000001) << 2) |
                       (((inst >> 10) & 0x00000007) << 3));
      ret = inst_lw(rd, rs1, offs);
      break;
    }
    case 0b110: { // SW
      unsigned offs = ((((inst >> 5) & 0x00000001) << 6) |
                       (((inst >> 6) & 0x00000001) << 2) |
                       (((inst >> 10) & 0x00000007) << 3));
      ret = inst_sw(rs1, rs2, offs);
      break;
    }
    default:
      ret = 0;
      break;
    }
    break;
  }
  case 0x01:
    // quadrant 1
    switch ((inst >> 13) & 0x7) {
    case 0b000: { // ADDI
      unsigned rs1 = (inst >> 7) & 0x1f;
      unsigned rd = rs1;
      unsigned imm = ((inst & 0x00001000) ? 0xfffffe0 : 0) | ((inst >> 2) & 0x01f);
      ret = inst_addi(rd, rs1, imm);
      break;
    }
    case 0b001: { // JAL
      unsigned offs = ((inst & 0x00001000) ? 0xfffff800 : 0) |
        (((inst >> 2) & 0x01) << 5) |
        (((inst >> 3) & 0x07) << 1) |
        (((inst >> 6) & 0x01) << 7) |
        (((inst >> 7) & 0x01) << 6) |
        (((inst >> 8) & 0x01) << 10) |
        (((inst >> 9) & 0x03) << 8) |
        (((inst >> 11) & 0x01) << 4);
      ret = inst_jal(REG_RA, offs);
      break;
    }
    case 0b010: { // LI
      unsigned rd = (inst >> 7) & 0x1f;
      unsigned imm = ((inst & 0x00001000) ? 0xfffffe0 : 0) | ((inst >> 2) & 0x01f);
      ret = inst_addi(rd, REG_ZERO, imm);
      break;
    }
    case 0b011: {
      unsigned rd = (inst >> 7) & 0x1f;
      if (rd == REG_SP) { // ADDI16SP
        unsigned imm = ((inst & 0x00001000) ? 0xfffffe00 : 0) |
          (((inst >> 2) & 0x1) << 5) |
          (((inst >> 3) & 0x3) << 7) |
          (((inst >> 5) & 0x1) << 6) |
          (((inst >> 6) & 0x1) << 4);
        ret = inst_addi(REG_SP, REG_SP, imm);
      } else { // LUI
        unsigned imm = ((inst & 0x00001000) ? 0xfffe0000 : 0) | (((inst >> 2) & 0x1f) << 12);
        ret = inst_lui(rd, imm);
      }
      break;
    }
    case 0b100: { // other arithmetics
      switch ((inst >> 10) & 0x03) {
      case 0b00: { // SRLI
        unsigned rs1 = ((inst >> 7) & 0x07) | 0x08;
        unsigned rd = rs1;
        unsigned shamt = (((inst >> 12) & 0x00000001) << 5) | ((inst >> 2) & 0x0000001f);
        ret = inst_srli(rd, rs1, shamt);
        break;
      }
      case 0b01: { // SRAI
        unsigned rs1 = ((inst >> 7) & 0x07) | 0x08;
        unsigned rd = rs1;
        unsigned shamt = (((inst >> 12) & 0x00000001) << 5) | ((inst >> 2) & 0x0000001f);
        ret = inst_srai(rd, rs1, shamt);
        break;
      }
      case 0b10: { // ANDI
        unsigned rs1 = ((inst >> 7) & 0x07) | 0x08;
        unsigned rd = rs1;
        unsigned imm = ((inst & 0x00001000) ? 0xffffffe0 : 0) | ((inst >> 2) & 0x1f);
        ret = inst_andi(rd, rs1, imm);
        break;
      }
      default: {
        unsigned rs1 = ((inst >> 7) & 0x07) | 0x08;
        unsigned rs2 = ((inst >> 2) & 0x07) | 0x08;
        unsigned rd = rs1;
        switch ((inst >> 5) & 0x03) {
        case 0b00: // SUB
          ret = inst_sub(rd, rs1, rs2);
          break;
        case 0b01: // XOR
          ret = inst_xor(rd, rs1, rs2);
          break;
        case 0b10: // OR
          ret = inst_or(rd, rs1, rs2);
          break;
        case 0b11: // AND
          ret = inst_and(rd, rs1, rs2);
          break;
        }
      }
      }
      break;
    }
    case 0b101: { // JUMP
      unsigned offs = ((inst & 0x00001000) ? 0xfffff800 : 0) |
        (((inst >> 2) & 0x01) << 5) |
        (((inst >> 3) & 0x07) << 1) |
        (((inst >> 6) & 0x01) << 7) |
        (((inst >> 7) & 0x01) << 6) |
        (((inst >> 8) & 0x01) << 10) |
        (((inst >> 9) & 0x03) << 8) |
        (((inst >> 11) & 0x01) << 4);
      ret = inst_jal(REG_ZERO, offs);
      break;
    }
    case 0b110: { // BEQZ
      unsigned rs1 = ((inst >> 7) & 0x07) | 0x08;
      unsigned offs = ((inst & 0x00001000) ? 0xffffff00 : 0) |
        (((inst >> 2) & 0x01) << 5) |
        (((inst >> 3) & 0x03) << 1) |
        (((inst >> 5) & 0x03) << 6) |
        (((inst >> 10) & 0x03) << 3);
      ret = inst_beq(rs1, REG_ZERO, offs);
      break;
    }
    case 0b111: { // BNEZ
      unsigned rs1 = ((inst >> 7) & 0x07) | 0x08;
      unsigned offs = ((inst & 0x00001000) ? 0xffffff00 : 0) |
        (((inst >> 2) & 0x01) << 5) |
        (((inst >> 3) & 0x03) << 1) |
        (((inst >> 5) & 0x03) << 6) |
        (((inst >> 10) & 0x03) << 3);
      ret = inst_bne(rs1, REG_ZERO, offs);
      break;
    }
    default:
      ret = 0;
      break;
    }
    break;
  case 0x02:
    // quadrent 2
    switch ((inst >> 13) & 0x7) {
    case 0b000: { // SLLI
      unsigned rs1 = (inst >> 7) & 0x0000001f;
      unsigned rd = rs1;
      unsigned shamt = (((inst >> 12) & 0x00000001) << 5) | ((inst >> 2) & 0x0000001f);
      ret = inst_slli(rd, rs1, shamt);
      break;
    }
    case 0b010: { // Load Word with Stack Pointer
      unsigned rd = (inst >> 7) & 0x1f;
      unsigned offs = ((((inst >> 2) & 0x03) << 6) |
                       (((inst >> 4) & 0x07) << 2) |
                       (((inst >> 12) & 0x01) << 5));
      ret = inst_lw(rd, REG_SP, offs);
      break;
    }
    case 0b100: {
      unsigned rs1 = (inst >> 7) & 0x1f;
      unsigned rs2 = (inst >> 2) & 0x1f;
      unsigned rd = rs1;
      if ((inst & 0x1000) == 0) {
        if (rs2 != 0) {
          ret = inst_addi(rd, rs2, 0); // MV
        } else {
          ret = inst_jalr(REG_ZERO, rs1, 0); // JR
        }
      } else {
        // EBREAK, JALR, ADD
        if (rs1 == 0 && rs2 == 0) {
          ret = inst_ebreak();
        } else if (rs1 != 0 && rs2 == 0) {
          ret = inst_jalr(REG_RA, rs1, 0); // JALR
        } else if (rs1 != 0 && rs2 != 0) {
          ret = inst_add(rd, rs1, rs2); // ADD
        }
        // HINT for rs1 is 0
      }
      break;
    }
    case 0b110: { // Store Word with Stack Pointer
      unsigned rs2 = (inst >> 2) & 0x1f;
      unsigned offs = ((((inst >> 7) & 0x03) << 6) |
                       (((inst >> 9) & 0x0f) << 2));
      ret = inst_sw(REG_SP, rs2, offs);
      break;
    }
    case 0b001:
    case 0b011:
    case 0b101:
    case 0b111:
    default:
      ret = 0;
      break;
    }
    break;
  default:
    ret = inst;
    break;
  }
  return ret;
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

void core_fetch_instruction(core_t *core, unsigned pc, struct core_step_result *result, unsigned prv) {
  // hit the instruction fetch window
  int found = 0;
  unsigned inst = 0;
  unsigned exception = 0;
  for (int i = 0; i < CORE_WINDOW_SIZE; i++) {
    if (core->window.pc[i] == pc) {
      found = 1;
      inst = core->window.inst[i];
      exception = core->window.exception[i];
      break;
    }
  }
  if (!found) {
    unsigned window_pc = pc;
    unsigned paddr;
    unsigned char *line = NULL;
    unsigned index = 0;
    exception = memory_address_translation(core->mem, window_pc, &paddr, ACCESS_TYPE_INSTRUCTION, prv);
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
        if (index + 3 > core->mem->icache->line_mask) {
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
        exception = core->window.exception[i];
        break;
      }
    }
  }
  result->inst = inst;
  result->exception_code = exception;
  return;
}

void core_step(core_t *core, unsigned pc, struct core_step_result *result, unsigned prv) {
  // init result
  result->pc = pc;
  result->cycle = core->csr->cycle;
  result->prv = prv;
  result->pc_next = pc;
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
    inst = decompress(result->inst);
  }
  opcode = get_opcode(inst);
  // exec
  switch (opcode) {
  case OPCODE_OP_IMM: {
    result->rd_regno = get_rd(inst);
    result->rs1_regno = get_rs1(inst);
    unsigned src1 = core->gpr[result->rs1_regno];
    unsigned src2 = get_immediate(inst);
    process_alu(get_funct3(inst), src1, src2, 0, result);
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
      result->rd_data = csr_csrrw(core->csr, get_csr_addr(inst), core->gpr[result->rs1_regno]);
      break;
    case 0x2: // READ_SET
      result->rs1_regno = get_rs1(inst);
      result->rd_data = csr_csrrs(core->csr, get_csr_addr(inst), core->gpr[result->rs1_regno]);
      break;
    case 0x3: // READ_CLEAR
      result->rs1_regno = get_rs1(inst);
      result->rd_data = csr_csrrc(core->csr, get_csr_addr(inst), core->gpr[result->rs1_regno]);
      break;
    case 0x4: // Hypervisor Extension
      result->exception_code = TRAP_CODE_ILLEGAL_INSTRUCTION;
      break;
    case 0x5: // READ_WRITE (imm)
      result->rd_data = csr_csrrw(core->csr, get_csr_addr(inst), get_csr_imm(inst));
      break;
    case 0x6: // READ_SET (imm)
      result->rd_data = csr_csrrs(core->csr, get_csr_addr(inst), get_csr_imm(inst));
      break;
    case 0x7: // READ_CLEAR (imm)
      result->rd_data = csr_csrrc(core->csr, get_csr_addr(inst), get_csr_imm(inst));
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
  return;
}

void core_fini(core_t *core) {
  free(core->window.pc);
  free(core->window.inst);
  free(core->window.exception);
  return;
}
