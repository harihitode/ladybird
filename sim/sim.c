#include "sim.h"
#include "elfloader.h"
#include "memory.h"
#include "csr.h"
#include "mmio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIGTRAP 5

void sim_init(sim_t *sim) {
  // clear gpr
  sim->registers = (unsigned *)calloc(NUM_REGISTERS, sizeof(unsigned));
  // init csr
  sim->csr = (csr_t *)malloc(sizeof(csr_t));
  csr_init(sim->csr);
  csr_set_sim(sim->csr, sim);
  sim->signum = SIGTRAP;
  sim->dbg_mode = 0;
  // init memory
  sim->mem = (memory_t *)malloc(sizeof(memory_t));
  memory_init(sim->mem, 128 * 1024 * 1024, 4 * 1024);
  memory_set_sim(sim->mem, sim);
  sim->reginfo = (char **)calloc(NUM_REGISTERS, sizeof(char *));
  for (int i = 0; i < NUM_REGISTERS; i++) {
    char *buf = (char *)malloc(128 * sizeof(char));
    if (i >= 0 && i <= 31) {
      sprintf(buf,
              "name:x%d;bitsize:32;offset:%d;format:hex;set:General Purpose "
              "Registers;",
              i, i * 4);
      if (i == 2) {
        sprintf(&buf[strlen(buf)], "alt-name:sp;generic:sp;");
      } else if (i == 8) {
        sprintf(&buf[strlen(buf)], "alt-name:fp;generic:fp;");
      }
    } else if (i == 32) {
      sprintf(buf, "name:pc;bitsize:32;offset:128;format:hex;set:General "
              "Purpose Registers;generic:pc;");
    }
    sim->reginfo[i] = buf;
  }
  sprintf(sim->triple, "%s", "riscv32-unknown-unknown-elf");
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
    for (unsigned j = 0; j < sim->elf->program_mem_size[i]; j++) {
      if (j < sim->elf->program_file_size[i]) {
        memory_store(sim->mem, sim->elf->program_base[i] + j, sim->elf->program[i][j], 1, 0);
      } else {
        // zero clear for BSS
        memory_store(sim->mem, sim->elf->program_base[i] + j, 0x00, 1, 0);
      }
    }
  }
  // set entry program counter
  sim->registers[REG_PC] = sim->elf->entry_address;
  ret = 0;
 cleanup:
  elf_fini(sim->elf);
  free(sim->elf);
  sim->elf = NULL;
  return ret;
}

void sim_fini(sim_t *sim) {
  free(sim->registers);
  memory_fini(sim->mem);
  free(sim->mem);
  csr_fini(sim->csr);
  free(sim->csr);
  for (int i = 0; i < NUM_REGISTERS; i++) {
    free(sim->reginfo[i]);
  }
  free(sim->reginfo);
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

#define REG_ZERO 0x00
#define REG_LINK 0x01
#define REG_SP 0x02

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
      ret = inst_jal(REG_LINK, offs);
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
          ret = inst_jalr(REG_LINK, rs1, 0); // JALR
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

struct result_t {
  unsigned rd;
  unsigned rd_data;
  unsigned pc_next;
};

void process_alu(sim_t *sim, unsigned funct, unsigned src1, unsigned src2, unsigned alt, struct result_t *result) {
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

void process_muldiv(sim_t *sim, unsigned funct, unsigned src1, unsigned src2, struct result_t *result) {
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

void sim_step(sim_t *sim) {
  // fetch
  unsigned inst = memory_load_instruction(sim->mem, sim->registers[REG_PC]);
  struct result_t result;
  result.rd = 0;
  unsigned opcode;
  unsigned instret = 1;
  if (sim->csr->exception) {
    goto csr_update;
  }
  if ((inst & 0x03) == 0x03) {
    result.pc_next = sim->registers[REG_PC] + 4;
  } else {
    result.pc_next = sim->registers[REG_PC] + 2;
    inst = decompress(inst);
  }
  opcode = get_opcode(inst);
  switch (opcode) {
  case OPCODE_OP_IMM: {
    result.rd = get_rd(inst);
    unsigned src1 = sim_read_register(sim, get_rs1(inst));
    unsigned src2 = get_immediate(inst);
    process_alu(sim, get_funct3(inst), src1, src2, 0, &result);
    break;
  }
  case OPCODE_OP: {
    result.rd = get_rd(inst);
    unsigned src1 = sim_read_register(sim, get_rs1(inst));
    unsigned src2 = sim_read_register(sim, get_rs2(inst));
    if (get_funct7(inst) == 0x01) {
      process_muldiv(sim, get_funct3(inst), src1, src2, &result);
    } else {
      process_alu(sim, get_funct3(inst), src1, src2, (inst & 0x40000000), &result);
    }
    break;
  }
  case OPCODE_AUIPC:
    result.rd = get_rd(inst);
    result.rd_data = sim->registers[REG_PC] + (inst & 0xfffff000);
    break;
  case OPCODE_LUI:
    result.rd = get_rd(inst);
    result.rd_data = (inst & 0xfffff000);
    break;
  case OPCODE_JALR:
    result.rd = get_rd(inst);
    result.rd_data = result.pc_next;
    result.pc_next = sim_read_register(sim, get_rs1(inst)) + get_jalr_offset(inst);
    break;
  case OPCODE_JAL:
    result.rd = get_rd(inst);
    result.rd_data = result.pc_next;
    result.pc_next = sim->registers[REG_PC] + get_jal_offset(inst);
    break;
  case OPCODE_STORE: {
    unsigned addr = sim_read_register(sim, get_rs1(inst)) + get_store_offset(inst);
    unsigned data = sim_read_register(sim, get_rs2(inst));
    switch (get_funct3(inst)) {
    case 0x0:
      memory_store(sim->mem, addr, data, 1, 0);
      break;
    case 0x1:
      memory_store(sim->mem, addr, data, 2, 0);
      break;
    case 0x2:
      memory_store(sim->mem, addr, data, 4, 0);
      break;
    default:
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    break;
  }
  case OPCODE_LOAD: {
    result.rd = get_rd(inst);
    unsigned addr = sim_read_register(sim, get_rs1(inst)) + get_load_offset(inst);
    switch (get_funct3(inst)) {
    case 0x0: // singed ext byte
      result.rd_data = (int)((char)memory_load(sim->mem, addr, 1, 0));
      break;
    case 0x1: // signed ext half
      result.rd_data = (int)((short)memory_load(sim->mem, addr, 2, 0));
      break;
    case 0x2:
      result.rd_data = memory_load(sim->mem, addr, 4, 0);
      break;
    case 0x4:
      result.rd_data = (unsigned char)memory_load(sim->mem, addr, 1, 0);
      break;
    case 0x5:
      result.rd_data = (unsigned short)memory_load(sim->mem, addr, 2, 0);
      break;
    default:
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    break;
  }
  case OPCODE_AMO: {
    result.rd = get_rd(inst);
    unsigned src1 = sim_read_register(sim, get_rs1(inst));
    unsigned src2 = sim_read_register(sim, get_rs2(inst));
    switch (get_funct5(inst)) {
    case 0x002: // Load Reserved
      result.rd_data = memory_load_reserved(sim->mem, src1);
      break;
    case 0x003: // Store Conditional
      result.rd_data = memory_store_conditional(sim->mem, src1, src2);
      break;
    case 0x000: // AMOADD
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, result.rd_data + src2, 4, 0);
      break;
    case 0x001: // AMOSWAP
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, src2, 4, 0);
      break;
    case 0x004: // AMOXOR
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, result.rd_data ^ src2, 4, 0);
      break;
    case 0x008: // AMOOR
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, result.rd_data | src2, 4, 0);
      break;
    case 0x00c: // AMOAND
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, result.rd_data & src2, 4, 0);
      break;
    case 0x010: // AMOMIN
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, ((int)result.rd_data < (int)src2) ? result.rd_data : src2, 4, 0);
      break;
    case 0x014: // AMOMAX
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, ((int)result.rd_data < (int)src2) ? src2 : result.rd_data, 4, 0);
      break;
    case 0x018: // AMOMINU
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, (result.rd_data < src2) ? result.rd_data : src2, 4, 0);
      break;
    case 0x01c: // AMOMAXU
      result.rd_data = memory_load(sim->mem, src1, 4, 0);
      memory_store(sim->mem, src1, (result.rd_data < src2) ? src2 : result.rd_data, 4, 0);
      break;
    default:
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    break;
  }
  case OPCODE_MISC_MEM:
    // any fence means cache flush for this system
    memory_dcache_write_back(sim->mem);
    break;
  case OPCODE_SYSTEM:
    if (get_funct3(inst) & 0x03) {
      result.rd = get_rd(inst);
    }
    // CSR OPERATIONS
    switch (get_funct3(inst)) {
    case 0x1: // READ_WRITE
      result.rd_data = csr_csrrw(sim->csr, get_csr_addr(inst), sim_read_register(sim, get_rs1(inst)));
      break;
    case 0x2: // READ_SET
      result.rd_data = csr_csrrs(sim->csr, get_csr_addr(inst), sim_read_register(sim, get_rs1(inst)));
      break;
    case 0x3: // READ_CLEAR
      result.rd_data = csr_csrrc(sim->csr, get_csr_addr(inst), sim_read_register(sim, get_rs1(inst)));
      break;
    case 0x4: // Hypervisor Extension
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    case 0x5: // READ_WRITE (imm)
      result.rd_data = csr_csrrw(sim->csr, get_csr_addr(inst), get_rs1(inst));
      break;
    case 0x6: // READ_SET (imm)
      result.rd_data = csr_csrrs(sim->csr, get_csr_addr(inst), get_rs1(inst));
      break;
    case 0x7: // READ_CLEAR (imm)
      result.rd_data = csr_csrrc(sim->csr, get_csr_addr(inst), get_rs1(inst));
      break;
    default: // OTHER SYSTEM OPERATIONS (ECALL, EBREAK, MRET, etc.)
      switch (get_funct7(inst)) {
      case 0x00:
        instret = 0;
        if (get_rs2(inst) == 0) {
          if (sim->csr->mode == PRIVILEGE_MODE_M) {
            csr_exception(sim->csr, TRAP_CODE_ENVIRONMENT_CALL_M);
          } else if (sim->csr->mode == PRIVILEGE_MODE_S) {
            csr_exception(sim->csr, TRAP_CODE_ENVIRONMENT_CALL_S);
          } else {
            csr_exception(sim->csr, TRAP_CODE_ENVIRONMENT_CALL_U);
          }
        } else if (get_rs2(inst) == 1) {
          if (sim->dbg_mode) {
            sim->signum = SIGTRAP;
          } else {
            csr_exception(sim->csr, TRAP_CODE_BREAKPOINT);
          }
        } else {
          csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
        }
        break;
      case 0x18:
        if (get_rs2(inst) == 2) {
          // MRET
          csr_trapret(sim->csr);
        } else {
          csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
        }
        break;
      case 0x08:
        if (get_rs2(inst) == 2) {
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
  case OPCODE_BRANCH: {
    // read
    unsigned src1 = sim_read_register(sim, get_rs1(inst));
    unsigned src2 = sim_read_register(sim, get_rs2(inst));
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
      csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
      break;
    }
    if (pred == 1) {
      result.pc_next = sim->registers[REG_PC] + get_branch_offset(inst);
    }
    break;
  }
  default:
    // invalid opcode
    csr_exception(sim->csr, TRAP_CODE_ILLEGAL_INSTRUCTION);
    break;
  }
  if (sim->csr->exception || sim->csr->trapret) {
    goto csr_update;
  }
  // commit
  if (result.rd != 0) {
    sim_write_register(sim, result.rd, result.rd_data);
  }
  sim->registers[REG_PC] = result.pc_next;
 csr_update:
  csr_cycle(sim->csr, instret);
  return;
}

unsigned sim_read_register(sim_t *sim, unsigned regno) {
  if (regno == 0) {
    return 0;
  } else if (regno < NUM_REGISTERS) {
    return sim->registers[regno];
  } else {
    return 0;
  }
}

void sim_write_register(sim_t *sim, unsigned regno, unsigned value) {
  if (regno < NUM_REGISTERS) {
    sim->registers[regno] = value;
  }
  return;
}

char sim_read_memory(sim_t *sim, unsigned addr) {
  unsigned ret = memory_load(sim->mem, addr, 1, 0);
  return ret;
}

void sim_write_memory(sim_t *sim, unsigned addr, char value) {
  memory_store(sim->mem, addr, value, 1, 0);
  memory_dcache_write_back(sim->mem);
  memory_icache_invalidate(sim->mem);
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

unsigned sim_get_instruction(sim_t *sim, unsigned pc) {
  return memory_load_instruction(sim->mem, pc);
}

void sim_clear_exception(sim_t *sim) {
  sim->csr->exception = 0;
}

void sim_debug_enable(sim_t *sim) {
  sim->dbg_mode = 1;
}

void sim_debug_continue(sim_t *sim) {
  // continue to ebreak
  sim->signum = 0;
  sim_clear_exception(sim);
  while (sim->signum == 0) {
    sim_step(sim);
  }
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
  fprintf(stderr, "PC: %08x, (RAM: %08x, %08x)\n", sim->registers[REG_PC], memory_address_translation(sim->mem, sim->registers[REG_PC], 0), memory_load_instruction(sim->mem, sim->registers[REG_PC]));
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
