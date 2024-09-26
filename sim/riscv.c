#include "riscv.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ALT_INST 0x40000000

static unsigned inst_illegal() {
  return 0;
}

#if C_EXTENSION
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
  return (rs2 << 20) | (rs1 << 15) | (0b000 << 12) | (rd << 7) | OPCODE_OP | ALT_INST;
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
  return ((shamt & 0x3f) << 20) | (rs1 << 15) | (0b101 << 12) | (rd << 7) | OPCODE_OP_IMM | ALT_INST;
}

#if F_EXTENSION
static unsigned inst_flw(unsigned rd, unsigned base, unsigned offs) {
  return (offs << 20) | (base << 15) | (0b010 << 12) | (rd << 7) | OPCODE_LOAD_FP;
}

static unsigned inst_fsw(unsigned base, unsigned src, unsigned offs) {
  return ((offs & 0x0fe0) << 20) | (src << 20) | (base << 15) | (0b010 << 12) |
    ((offs & 0x01f) << 7) | OPCODE_STORE_FP;
}
#endif
#endif

unsigned riscv_decompress(unsigned inst) {
  unsigned ret = inst_illegal();
  switch (inst & 0x03) {
#if C_EXTENSION
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
    case 0b011: { // FLW
#if F_EXTENSION
      unsigned offs = ((((inst >> 5) & 0x00000001) << 6) |
                       (((inst >> 6) & 0x00000001) << 2) |
                       (((inst >> 10) & 0x00000007) << 3));
      ret = inst_flw(rd, rs1, offs);
#endif
      break;
    }
    case 0b110: { // SW
      unsigned offs = ((((inst >> 5) & 0x00000001) << 6) |
                       (((inst >> 6) & 0x00000001) << 2) |
                       (((inst >> 10) & 0x00000007) << 3));
      ret = inst_sw(rs1, rs2, offs);
      break;
    }
    case 0b111: { // FSW
#if F_EXTENSION
      unsigned offs = ((((inst >> 5) & 0x00000001) << 6) |
                       (((inst >> 6) & 0x00000001) << 2) |
                       (((inst >> 10) & 0x00000007) << 3));
      ret = inst_fsw(rs1, rs2, offs);
#endif
      break;
    }
    default:
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
    case 0b010: { // Load Word to GPR with Stack Pointer
      unsigned rd = (inst >> 7) & 0x1f;
      unsigned offs = ((((inst >> 2) & 0x03) << 6) |
                       (((inst >> 4) & 0x07) << 2) |
                       (((inst >> 12) & 0x01) << 5));
      ret = inst_lw(rd, REG_SP, offs);
      break;
    }
    case 0b011: { // Load Word to FPR with Stack Pointer
#if F_EXTENSION
      unsigned rd = (inst >> 7) & 0x1f;
      unsigned offs = ((((inst >> 2) & 0x03) << 6) |
                       (((inst >> 4) & 0x07) << 2) |
                       (((inst >> 12) & 0x01) << 5));
      ret = inst_flw(rd, REG_SP, offs);
#endif
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
    case 0b110: { // Store Word from GPR with Stack Pointer
      unsigned rs2 = (inst >> 2) & 0x1f;
      unsigned offs = ((((inst >> 7) & 0x03) << 6) |
                       (((inst >> 9) & 0x0f) << 2));
      ret = inst_sw(REG_SP, rs2, offs);
      break;
    }
    case 0b111: { // Store Word from FPR with Stack Pointer
#if F_EXTENSION
      unsigned rs2 = (inst >> 2) & 0x1f;
      unsigned offs = ((((inst >> 7) & 0x03) << 6) |
                       (((inst >> 9) & 0x0f) << 2));
      ret = inst_fsw(REG_SP, rs2, offs);
#endif
      break;
    }
    case 0b001:
    case 0b101:
    default:
      break;
    }
    break;
#endif
  case 0x03:
    ret = inst;
    break;
  }
  return ret;
}

const char *riscv_get_extension_string() {
  static char buf[256];
  int z_seperate = 0;
  if (E_EXTENSION) {
    sprintf(buf, "RV%dE", XLEN);
  } else {
    sprintf(buf, "RV%dI", XLEN);
  }
  if (M_EXTENSION) {
    sprintf(&buf[strlen(buf)], "M");
  }
  if (A_EXTENSION) {
    sprintf(&buf[strlen(buf)], "A");
  }
  if (F_EXTENSION && !D_EXTENSION) {
    sprintf(&buf[strlen(buf)], "F");
  }
  if (D_EXTENSION) {
    sprintf(&buf[strlen(buf)], "D");
  }
  if (C_EXTENSION) {
    sprintf(&buf[strlen(buf)], "C");
  }
  if (V_EXTENSION) {
    sprintf(&buf[strlen(buf)], "V");
  }
  if (Z_ICSR_EXTENSION) {
    if (z_seperate++) sprintf(&buf[strlen(buf)], "_");
    sprintf(&buf[strlen(buf)], "Zicsr");
  }
  if (Z_IFENCEI_EXTENSION) {
    if (z_seperate++) sprintf(&buf[strlen(buf)], "_");
    sprintf(&buf[strlen(buf)], "Zifencei");
  }
  return buf;
}

const char *riscv_get_mnemonic(unsigned inst) {
  static char buf[128];
  sprintf(buf, "ILLEGAL");
  unsigned funct3 = (inst >> 12) & 0x00000007;
  unsigned funct5 = (inst >> 27) & 0x0000001f;
  unsigned funct7 = (inst >> 25) & 0x0000007f;
  unsigned rs2 = (inst >> 20) & 0x0000001f;
  unsigned alternate = (inst & ALT_INST);
  switch (inst & 0x7f) {
  case OPCODE_LOAD:
    switch (funct3) {
    case 0x0:
      sprintf(buf, "LB");
      break;
    case 0x1:
      sprintf(buf, "LH");
      break;
    case 0x2:
      sprintf(buf, "LW");
      break;
    case 0x4:
      sprintf(buf, "LBU");
      break;
    case 0x5:
      sprintf(buf, "LHU");
      break;
    default:
      break;
    }
    break;
  case OPCODE_LOAD_FP:
    sprintf(buf, "FLW");
    break;
  case OPCODE_MISC_MEM: {
    unsigned char fm = ((inst >> 28) & 0x0f);
    unsigned rs1 = ((inst >> 15) & 0x1f);
    unsigned rd = ((inst >> 7) & 0x1f);
    unsigned pred = ((inst >> 24) & 0x0f);
    unsigned succ = ((inst >> 20) & 0x0f);
    if (funct3 == 0x0 && rs1 == 0 && rd == 0 && pred != 0 && succ != 0) {
      if (fm == 0) {
        sprintf(buf, "FENCE.");
        if (inst & FENCE_PRED_I) sprintf(&buf[strlen(buf)], "I");
        if (inst & FENCE_PRED_O) sprintf(&buf[strlen(buf)], "O");
        if (inst & FENCE_PRED_R) sprintf(&buf[strlen(buf)], "R");
        if (inst & FENCE_PRED_W) sprintf(&buf[strlen(buf)], "W");
        sprintf(&buf[strlen(buf)], ",");
        if (inst & FENCE_SUCC_I) sprintf(&buf[strlen(buf)], "I");
        if (inst & FENCE_SUCC_O) sprintf(&buf[strlen(buf)], "O");
        if (inst & FENCE_SUCC_R) sprintf(&buf[strlen(buf)], "R");
        if (inst & FENCE_SUCC_W) sprintf(&buf[strlen(buf)], "W");
      } else if (fm == 0x08 && pred == 0x3 && succ == 0x3) {
        sprintf(buf, "FENCE.TSO");
      }
    } else if (funct3 == 0x1) {
      sprintf(buf, "FENCE.I");
    }
    break;
  }
  case OPCODE_OP_IMM:
    switch (funct3) {
    case 0x0:
      sprintf(buf, "ADDI");
      break;
    case 0x1:
      sprintf(buf, "SLLI");
      break;
    case 0x2:
      sprintf(buf, "SLTI");
      break;
    case 0x3:
      sprintf(buf, "SLTUI");
      break;
    case 0x4:
      sprintf(buf, "XORI");
      break;
    case 0x5:
      if (alternate) {
        sprintf(buf, "SRAI");
      } else {
        sprintf(buf, "SRLI");
      }
      break;
    case 0x6:
      sprintf(buf, "ORI");
      break;
    case 0x7:
      sprintf(buf, "ANDI");
      break;
    }
    break;
  case OPCODE_AUIPC:
    sprintf(buf, "AUIPC");
    break;
  case OPCODE_STORE:
    switch (funct3) {
    case 0x0:
      sprintf(buf, "SB");
      break;
    case 0x1:
      sprintf(buf, "SH");
      break;
    case 0x2:
      sprintf(buf, "SW");
      break;
    default:
      break;
    }
    break;
  case OPCODE_STORE_FP:
    sprintf(buf, "FSW");
    break;
  case OPCODE_AMO:
    switch (funct5) {
    case 0x002: // Load Reserved
      sprintf(buf, "LR");
      break;
    case 0x003: // Store Conditional
      sprintf(buf, "SC");
      break;
    case 0x000: // AMOADD
      sprintf(buf, "AMOADD");
      break;
    case 0x001: // AMOSWAP
      sprintf(buf, "AMOSWAP");
      break;
    case 0x004: // AMOXOR
      sprintf(buf, "AMOXOR");
      break;
    case 0x008: // AMOOR
      sprintf(buf, "AMOOR");
      break;
    case 0x00c: // AMOAND
      sprintf(buf, "AMOAND");
      break;
    case 0x010: // AMOMIN
      sprintf(buf, "AMOMIN");
      break;
    case 0x014: // AMOMAX
      sprintf(buf, "AMOMAX");
      break;
    case 0x018: // AMOMINU
      sprintf(buf, "AMOMINU");
      break;
    case 0x01c: // AMOMAXU
      sprintf(buf, "AMOMAXU");
      break;
    default:
      break;
    }
    if (inst & AMO_AQ) {
      sprintf(&buf[strlen(buf)], ".AQ");
    }
    if (inst & AMO_RL) {
      sprintf(&buf[strlen(buf)], ".RL");
    }
    break;
  case OPCODE_OP:
    if (funct7 == 1) {
      switch (funct3) {
      case 0x0:
        sprintf(buf, "MUL");
        break;
      case 0x1: // MULH (extended: signed * signedb)
        sprintf(buf, "MULH");
        break;
      case 0x2: // MULHSU (extended: signed * unsigned)
        sprintf(buf, "MULHSU");
        break;
      case 0x3: // MULHU (extended: unsigned * unsigned)
        sprintf(buf, "MULHU");
        break;
      case 0x4: // DIV
        sprintf(buf, "DIV");
        break;
      case 0x5: // DIVU
        sprintf(buf, "DIVU");
        break;
      case 0x6: // REM
        sprintf(buf, "REM");
        break;
      case 0x7: // REMU
        sprintf(buf, "REMU");
        break;
      }
    } else {
      switch (funct3) {
      case 0x0:
        if (alternate) {
          sprintf(buf, "SUB");
        } else {
          sprintf(buf, "ADD");
        }
        break;
      case 0x1:
        sprintf(buf, "SLL");
        break;
      case 0x2:
        sprintf(buf, "SLT");
        break;
      case 0x3:
        sprintf(buf, "SLTU");
        break;
      case 0x4:
        sprintf(buf, "XOR");
        break;
      case 0x5:
        if (alternate) {
          sprintf(buf, "SRA");
        } else {
          sprintf(buf, "SRL");
        }
        break;
      case 0x6:
        sprintf(buf, "OR");
        break;
      case 0x7:
        sprintf(buf, "AND");
        break;
      }
    }
    break;
  case OPCODE_LUI:
    sprintf(buf, "LUI");
    break;
  case OPCODE_MADD:
    sprintf(buf, "FMADD.S");
    break;
  case OPCODE_MSUB:
    sprintf(buf, "FMSUB.S");
    break;
  case OPCODE_NMSUB:
    sprintf(buf, "FNMSUB.S");
    break;
  case OPCODE_NMADD:
    sprintf(buf, "FNMADD.S");
    break;
  case OPCODE_OP_FP:
    switch (funct7) {
    case 0x00: // FADD
      sprintf(buf, "FADD.S");
      break;
    case 0x04: // FSUB
      sprintf(buf, "FSUB.S");
      break;
    case 0x08: // FMUL
      sprintf(buf, "FMUL.S");
      break;
    case 0x0c: // FDIV
      sprintf(buf, "FDIV.S");
      break;
    case 0x2c: // FSQRT
      sprintf(buf, "FSQRT.S");
      break;
    case 0x10: // FSGNJ
      switch (funct3) {
      case 0x0: // Normal
        sprintf(buf, "FSGNJ.S");
        break;
      case 0x1: // Negative
        sprintf(buf, "FSGNJN.S");
        break;
      case 0x2: // Exclusive OR
        sprintf(buf, "FSGNJX.S");
        break;
      default:
        break;
      }
      break;
    case 0x14: // MINMAX
      switch (funct3) {
      case 0x0: // Minimum
        sprintf(buf, "FMIN.S");
        break;
      case 0x1: // Maximum
        sprintf(buf, "FMAX.S");
        break;
      default:
        break;
      }
      break;
    case 0x50: // Comparison
      switch (funct3) {
      case 0x0: // FLE
        sprintf(buf, "FLE.S");
        break;
      case 0x1: // FLT
        sprintf(buf, "FLT.S");
        break;
      case 0x2: // FEQ
        sprintf(buf, "FEQ.S");
        break;
      default:
        break;
      }
      break;
    case 0x60: // Convert to word
      switch (rs2) {
      case 0x0: // FCVT.W.S
        sprintf(buf, "FCVT.W.S");
        break;
      case 0x1: // FCVT.WU.S
        sprintf(buf, "FCVT.WU.S");
        break;
      default:
        break;
      }
      break;
    case 0x68: // Convert to float
      switch (rs2) {
      case 0x0: // FCVT.S.W
        sprintf(buf, "FCVT.S.W");
        break;
      case 0x1: // FCVT.S.WU
        sprintf(buf, "FCVT.S.WU");
        break;
      default:
        break;
      }
      break;
    case 0x70: // Move/Class
      switch (funct3) {
      case 0x0: // Move FPR to GPR
        sprintf(buf, "FMV.X.W");
        break;
      case 0x1: // Classification
        sprintf(buf, "FCLASS.S");
        break;
      default:
        break;
      }
      break;
    case 0x78: // Move GPR to FPR
      sprintf(buf, "FMV.W.X");
      break;
    default:
      break;
    }
    break;
  case OPCODE_BRANCH:
    switch (funct3) {
    case 0x0:
      sprintf(buf, "BEQ");
      break;
    case 0x1:
      sprintf(buf, "BNE");
      break;
    case 0x4:
      sprintf(buf, "BLT");
      break;
    case 0x5:
      sprintf(buf, "BGE");
      break;
    case 0x6:
      sprintf(buf, "BLTU");
      break;
    case 0x7:
      sprintf(buf, "BGEU");
      break;
    default:
      break;
    }
    break;
  case OPCODE_JALR:
    sprintf(buf, "JALR");
    break;
  case OPCODE_JAL:
    sprintf(buf, "JAL");
    break;
  case OPCODE_SYSTEM:
    switch (funct3) {
    case 0x1: // READ_WRITE
      sprintf(buf, "CSRRW");
      break;
    case 0x2: // READ_SET
      sprintf(buf, "CSRRS");
      break;
    case 0x3: // READ_CLEAR
      sprintf(buf, "CSRRC");
      break;
    case 0x5: // READ_WRITE (imm)
      sprintf(buf, "CSRRWI");
      break;
    case 0x6: // READ_SET (imm)
      sprintf(buf, "CSRRSI");
      break;
    case 0x7: // READ_CLEAR (imm)
      sprintf(buf, "CSRRCI");
      break;
    default: // OTHER SYSTEM OPERATIONS (ECALL, EBREAK, MRET, etc.)
      switch (funct7) {
      case 0x00:
        if (rs2 == 0) {
          sprintf(buf, "ECALL");
        } else if (rs2 == 1) {
          sprintf(buf, "EBREAK");
        }
        break;
      case 0x18:
        if (rs2 == 2) {
          sprintf(buf, "MRET");
        }
        break;
      case 0x08:
        if (rs2 == 2) {
          sprintf(buf, "SRET");
        } else if (rs2 == 5) {
          sprintf(buf, "WFI");
        }
        break;
      case 0x09:
        // SFENCE.VMA
        sprintf(buf, "SFENCE.VMA");
        break;
      default:
        break;
      }
    }
    break;
  default:
    break;
  }
  return buf;
}

static unsigned char fp_get_sign(unsigned a) {
  return (a >> 31) & 0x00000001;
}

static unsigned char fp_get_exponent(unsigned a) {
  return (a >> 23) & 0x000000ff;
}

static unsigned fp_get_mantissa(unsigned a) {
  return a & 0x007fffff;
}

static int riscv_issnan(unsigned src1) {
  if (((0x7f800000 & src1) == 0x7f800000) && ((0x00400000 & src1) == 0) && (0x003fffff & src1) != 0) {
    return 1;
  } else {
    return 0;
  }
}

static int riscv_isqnan(unsigned src1) {
  if (((0x7f800000 & src1) == 0x7f800000) && ((0x00400000 & src1) != 0)) {
    return 1;
  } else {
    return 0;
  }
}

static int riscv_isnan(unsigned src1) {
  return riscv_issnan(src1) | riscv_isqnan(src1);
}

static int riscv_iszero(unsigned src1) {
  if (fp_get_exponent(src1) == 0x00000000 && fp_get_mantissa(src1) == 0x0) {
    return (fp_get_sign(src1) == 1) ? -1 : 1;
  } else {
    return 0;
  }
}

static int riscv_isinf(unsigned src1) {
  if (fp_get_exponent(src1) == 0x000000ff && fp_get_mantissa(src1) == 0x0) {
    return (fp_get_sign(src1) == 1) ? -1 : 1;
  } else {
    return 0;
  }
}

static unsigned char riscv_inexact(unsigned long long mantissa) {
  unsigned char guard = (mantissa >> 31) & 0x01;
  unsigned char round = (mantissa >> 30) & 0x01;
  unsigned char sticky = (mantissa & 0x0000000003ffffff) ? 1 : 0;
  return (guard | round | sticky) ? 1 : 0;
}

static unsigned char riscv_rounding(unsigned long long mantissa, unsigned char sign_flag, unsigned char rm) {
  unsigned char roundup = 0;
  unsigned char lsb = (mantissa >> 32) & 0x01;
  unsigned char guard = (mantissa >> 31) & 0x01;
  unsigned char round = (mantissa >> 30) & 0x01;
  unsigned char sticky = (mantissa & 0x0000000003ffffff) ? 1 : 0;
  switch (rm) {
  case FEXT_ROUNDING_MODE_RNE:
    roundup = guard & (lsb | round | sticky);
    break;
  case FEXT_ROUNDING_MODE_RTZ: // round to zero
    roundup = 0;
    break;
  case FEXT_ROUNDING_MODE_RDN: // round to negative inf
    roundup = ((guard | round | sticky) & sign_flag) ? 1 : 0;
    break;
  case FEXT_ROUNDING_MODE_RUP: // round to positive inf
    roundup = ((guard | round | sticky) & (!sign_flag)) ? 1 : 0;
    break;
  case FEXT_ROUNDING_MODE_RMM: // round to magnitude (large abs)
    roundup = guard;
    break;
  default:
    roundup = 0;
    break;
  }
  return roundup;
}

static unsigned uint32_get_leadingzero(unsigned a) {
  unsigned leading_zero = 0;
  for (leading_zero = 0; leading_zero < 32; leading_zero++) {
    if (a & (0x80000000 >> leading_zero)) {
      break;
    }
  }
  return leading_zero;
}

struct uint128_t {
  unsigned a;
  unsigned b;
  unsigned c;
  unsigned d;
};

static unsigned uint128_get_leadingzero(struct uint128_t a) {
  unsigned leading_zero = 0;
  leading_zero += uint32_get_leadingzero(a.a);
  if (leading_zero != 32) goto fin;
  leading_zero += uint32_get_leadingzero(a.b);
  if (leading_zero != 64) goto fin;
  leading_zero += uint32_get_leadingzero(a.c);
  if (leading_zero != 92) goto fin;
  leading_zero += uint32_get_leadingzero(a.d);
fin:
  return leading_zero;
}

struct uint128_t uint128_shift_right(struct uint128_t src, int shamt) {
  struct uint128_t result;
  if (shamt >= 128 || shamt <= -128) {
    result.a = 0;
    result.b = 0;
    result.c = 0;
    result.d = 0;
  } else if (shamt == 0) {
    result = src;
  } else if (shamt < 128 && shamt >= 96) {
    result.a = 0;
    result.b = 0;
    result.c = 0;
    result.d = (unsigned long long)src.a >> (shamt - 96);
  } else if (shamt < 96 && shamt >= 64) {
    result.a = 0;
    result.b = 0;
    result.c = (unsigned long long)src.a >> (shamt - 64);
    result.d = ((unsigned long long)src.a << (64 - (shamt - 32))) | ((unsigned long long)src.b >> (shamt - 64));
  } else if (shamt < 64 && shamt >= 32) {
    result.a = 0;
    result.b = src.a >> (shamt - 32);
    result.c = ((unsigned long long)src.a << (32 - (shamt - 32))) | ((unsigned long long)src.b >> (shamt - 32));
    result.d = ((unsigned long long)src.b << (32 - (shamt - 32))) | ((unsigned long long)src.c >> (shamt - 32));
  } else if (shamt < 32 && shamt > 0) {
    result.a = (unsigned long long)src.a >> shamt;
    result.b = ((unsigned long long)src.a << (32 - shamt)) | ((unsigned long long)src.b >> shamt);
    result.c = ((unsigned long long)src.b << (32 - shamt)) | ((unsigned long long)src.c >> shamt);
    result.d = ((unsigned long long)src.c << (32 - shamt)) | ((unsigned long long)src.d >> shamt);
  } else if (shamt > -32 && shamt < 0) {
    result.a = ((unsigned long long)src.a << -shamt) | ((unsigned long long)src.b >> (32 + shamt));
    result.b = ((unsigned long long)src.b << -shamt) | ((unsigned long long)src.c >> (32 + shamt));
    result.c = ((unsigned long long)src.c << -shamt) | ((unsigned long long)src.d >> (32 + shamt));
    result.d = (unsigned long long)src.d << -shamt;
  } else if (shamt > -64 && shamt <= -32) {
    result.a = ((unsigned long long)src.b << (-32 - shamt)) | ((unsigned long long)src.c >> (64 + shamt));
    result.b = ((unsigned long long)src.c << (-32 - shamt)) | ((unsigned long long)src.d >> (64 + shamt));
    result.c = (unsigned long long)src.d << (-32 - shamt);
    result.d = 0;
  } else if (shamt > -96 && shamt <= -64) {
    result.a = ((unsigned long long)src.c << (-64 - shamt)) | ((unsigned long long)src.d >> (96 + shamt));
    result.b = (unsigned long long)src.d << (-64 - shamt);
    result.c = 0;
    result.d = 0;
  } else if (shamt > -128 && shamt <= -96) {
    result.a = (unsigned long long)src.d << (-96 - shamt);
    result.b = 0;
    result.c = 0;
    result.d = 0;
  } else {
    result.a = 0;
    result.b = 0;
    result.c = 0;
    result.d = 0;
  }
  return result;
}

struct uint128_t uint128_add(struct uint128_t src1, struct uint128_t src2) {
  struct uint128_t result;
  unsigned long long result_d = (unsigned long long)src1.d + (unsigned long long)src2.d;
  unsigned long long result_c = (unsigned long long)src1.c + (unsigned long long)src2.c + (result_d >> 32);
  unsigned long long result_b = (unsigned long long)src1.b + (unsigned long long)src2.b + (result_c >> 32);
  unsigned long long result_a = (unsigned long long)src1.a + (unsigned long long)src2.a + (result_b >> 32);
  result.a = result_a & 0xffffffff;
  result.b = result_b & 0xffffffff;
  result.c = result_c & 0xffffffff;
  result.d = result_d & 0xffffffff;
  return result;
}

struct uint128_t uint128_neg(struct uint128_t src1) {
  struct uint128_t plsone, result;
  plsone.a = 0;
  plsone.b = 0;
  plsone.c = 0;
  plsone.d = 1;
  src1.a = ~src1.a;
  src1.b = ~src1.b;
  src1.c = ~src1.c;
  src1.d = ~src1.d;
  result = uint128_add(src1, plsone);
  return result;
}

struct uint128_t uint128_sub(struct uint128_t src1, struct uint128_t src2) {
  struct uint128_t result;
  src2 = uint128_neg(src2);
  result = uint128_add(src1, src2);
  return result;
}

struct uint128_t uint128_mul(struct uint128_t src1, struct uint128_t src2) {
  struct uint128_t result;
  unsigned long long dd_0 = (unsigned long long)src1.d * src2.d;
  unsigned long long dc_32 = (unsigned long long)src1.d * src2.c;
  unsigned long long db_64 = (unsigned long long)src1.d * src2.b;
  unsigned long long da_96 = (unsigned long long)src1.d * src2.a;

  unsigned long long cd_32 = (unsigned long long)src1.c * src2.d;
  unsigned long long cc_64 = (unsigned long long)src1.c * src2.c;
  unsigned long long cb_96 = (unsigned long long)src1.c * src2.b;

  unsigned long long bd_64 = (unsigned long long)src1.b * src2.d;
  unsigned long long bc_96 = (unsigned long long)src1.b * src2.c;

  unsigned long long ad_96 = (unsigned long long)src1.a * src2.d;

  unsigned long long result_d = dd_0;
  unsigned long long result_c = dc_32 + cd_32 + (result_d >> 32);
  unsigned long long result_b = db_64 + cc_64 + bd_64 + (result_c >> 32);
  unsigned long long result_a = da_96 + cb_96 + bc_96 + ad_96 + (result_b >> 32);
  result.a = result_a & 0xffffffff;
  result.b = result_b & 0xffffffff;
  result.c = result_c & 0xffffffff;
  result.d = result_d & 0xffffffff;
  return result;
}

unsigned riscv_fmadd(unsigned src1, unsigned src2, unsigned src3, unsigned char rm, unsigned char *exception) {
  unsigned result = 0;
  unsigned char src1_sign = fp_get_sign(src1);
  unsigned char src2_sign = fp_get_sign(src2);
  unsigned char src3_sign = fp_get_sign(src3);
  unsigned char src1_exponent = fp_get_exponent(src1);
  unsigned char src2_exponent = fp_get_exponent(src2);
  unsigned char src3_exponent = fp_get_exponent(src3);
  unsigned src1_mantissa = fp_get_mantissa(src1);
  unsigned src2_mantissa = fp_get_mantissa(src2);
  unsigned src3_mantissa = fp_get_mantissa(src3);
  unsigned char is_subtract = (src1_sign ^ src2_sign ^ src3_sign) ? 1 : 0;

  unsigned char src1_is_zero = src1_exponent == 0 && src1_mantissa == 0;
  unsigned char src2_is_zero = src2_exponent == 0 && src2_mantissa == 0;
  unsigned char src3_is_zero = src3_exponent == 0 && src3_mantissa == 0;
  unsigned char src1_is_inf = src1_exponent == 255 && src1_mantissa == 0;
  unsigned char src2_is_inf = src2_exponent == 255 && src2_mantissa == 0;
  unsigned char src3_is_inf = src3_exponent == 255 && src3_mantissa == 0;
  unsigned char src1_is_nan = src1_exponent == 255 && src1_mantissa != 0;
  unsigned char src2_is_nan = src2_exponent == 255 && src2_mantissa != 0;
  unsigned char src3_is_nan = src3_exponent == 255 && src3_mantissa != 0;

  unsigned char result_is_nan = src1_is_nan || src2_is_nan || src3_is_nan ||
    (src1_is_zero && src2_is_inf) || (src1_is_inf && src2_is_zero) ||
    (is_subtract && (src1_is_inf || src2_is_inf) && src3_is_inf);
  unsigned char result_is_inf = src1_is_inf || src2_is_inf || src3_is_inf;
  unsigned char result_mul_sign = (src1_sign ^ src2_sign) ? 1 : 0;

  if (result_is_nan) {
    result = RISCV_CANONICAL_QNAN;
    if (exception) *exception |= FEXT_ACCURUED_EXCEPTION_NV;
  } else if (result_is_inf) {
    // infinite from inputs
    if (src3_is_inf) {
      result = (src3_sign << 31) | (0x000000ff << 23);
    } else {
      result = (result_mul_sign << 31) | (0x000000ff << 23);
    }
  } else {
    unsigned src1_exponent_value = (src1_exponent == 0) ? 1 : src1_exponent;
    unsigned src2_exponent_value = (src2_exponent == 0) ? 1 : src2_exponent;
    unsigned src3_exponent_value = (src3_exponent == 0) ? 1 : src3_exponent;
    unsigned char src1_is_subnormal = (src1_exponent == 0 && src1_mantissa != 0);
    unsigned char src2_is_subnormal = (src2_exponent == 0 && src2_mantissa != 0);
    unsigned char src3_is_subnormal = (src3_exponent == 0 && src3_mantissa != 0);
    unsigned src1_mantissa_value = src1_is_subnormal ? src1_mantissa : (src1_exponent != 0 ? (0x00800000 | src1_mantissa) : 0x0);
    unsigned src2_mantissa_value = src2_is_subnormal ? src2_mantissa : (src2_exponent != 0 ? (0x00800000 | src2_mantissa) : 0x0);
    unsigned src3_mantissa_value = src3_is_subnormal ? src3_mantissa : (src3_exponent != 0 ? (0x00800000 | src3_mantissa) : 0x0);
    unsigned result_mul_exponent = src1_exponent_value + src2_exponent_value - 127;
    int addend_shamt = src3_exponent_value - result_mul_exponent + 23; // 23: mantissa len
    unsigned char addend_sticky = (addend_shamt >= 0) ? 0 : ((addend_shamt < -26) ? (src3_mantissa_value != 0) : (src3_mantissa_value << (26 + addend_shamt)) != 0);
    unsigned char result_is_addend = ((addend_shamt > 49) || src1_is_zero || src2_is_zero) && !src3_is_zero;
    struct uint128_t mul_lhs, mul_rhs, addend, result_mul, result_fma, result_fma_abs, result_fma_shifted, result_fma_shifted_out;
    mul_lhs.a = 0;
    mul_lhs.b = 0;
    mul_lhs.c = 0;
    mul_lhs.d = src1_mantissa_value << 2;
    mul_rhs.a = 0;
    mul_rhs.b = 0;
    mul_rhs.c = 0;
    mul_rhs.d = src2_mantissa_value << 1;
    addend.a = 0;
    addend.b = 0;
    addend.c = 0;
    addend.d = src3_mantissa_value;
    addend = uint128_shift_right(addend, -51);
    addend = uint128_shift_right(addend, 49 - addend_shamt - 1); // 1 for sticky
    addend.d |= addend_sticky;
    result_mul = uint128_mul(mul_lhs, mul_rhs);
    if (is_subtract) {
      result_fma = uint128_sub(result_mul, addend);
    } else {
      result_fma = uint128_add(result_mul, addend);
    }
    unsigned char result_fma_sign = (result_fma.b & 0x00001000) ? 1 : 0; // result_fma[76]: sign bit
    result_fma_abs = (result_fma_sign) ? uint128_neg(result_fma) : result_fma;
    unsigned char result_sign = (result_mul_sign ^ result_fma_sign) ? 1 : 0;
    unsigned leadingzero = uint128_get_leadingzero(result_fma_abs) - (128 - 76);
    int result_exponent_value = result_mul_exponent - leadingzero + 26;
    unsigned char result_is_subnormal = (result_exponent_value <= 0) ? 1 : 0;
    int result_fma_shamt = (result_is_subnormal) ? 26 - result_mul_exponent : 51 - leadingzero;
    // calculate final result (shifting and rounding)
    result_fma_shifted = uint128_shift_right(result_fma_abs, result_fma_shamt);
    result_fma_shifted.d &= 0x00ffffff; // 24bit (shadow + mantissa)
    result_fma_shifted_out = uint128_shift_right(result_fma_abs, -(76 - result_fma_shamt));
    result_fma_shifted_out.a &= 0;
    result_fma_shifted_out.b &= 0x00000fff; // 76bit
    result_fma_shifted_out.c &= 0xffffffff;
    result_fma_shifted_out.d &= 0xffffffff;
    unsigned char result_lsb = (result_fma_shifted.d & 0x2) ? 1 : 0;
    unsigned char result_guard = (result_fma_shifted.d & 0x1) ? 1 : 0;
    unsigned char result_sticky = (result_fma_shifted_out.a != 0) || (result_fma_shifted_out.b != 0) || (result_fma_shifted_out.c != 0) || (result_fma_shifted_out.d != 0);
    unsigned char roundup = 0;
    switch (rm) {
    case FEXT_ROUNDING_MODE_RNE:
      roundup = result_guard && (result_lsb || result_sticky);
      break;
    case FEXT_ROUNDING_MODE_RTZ: // round to zero
      roundup = 0;
      break;
    case FEXT_ROUNDING_MODE_RDN: // round to negative inf
      roundup = ((result_guard | result_sticky) & result_sign) ? 1 : 0;
      break;
    case FEXT_ROUNDING_MODE_RUP: // round to positive inf
      roundup = ((result_guard | result_sticky) & (!result_sign)) ? 1 : 0;
      break;
    case FEXT_ROUNDING_MODE_RMM: // round to magnitude (large abs)
      roundup = result_guard;
      break;
    default:
      roundup = 0;
      break;
    }
    if (exception && (result_guard || result_sticky)) {
      *exception |= FEXT_ACCURUED_EXCEPTION_NX;
    }
    unsigned char expinc = (result_fma_shifted.d >= 0x00ffffff) ? 1 : 0;
    unsigned result_mantissa = ((result_fma_shifted.d >> 1) & 0x007fffff) + roundup;
    unsigned result_exponent = (result_is_subnormal) ? 0 : result_exponent_value + expinc;
    unsigned char result_is_zero = (result_fma.a == 0) && (result_fma.b == 0) && (result_fma.c == 0) && (result_fma.d == 0);
    if (result_exponent >= 255) {
      // go infinite during calculation
      result = (result_sign << 31) | (0x000000ff << 23);
    } else if (result_is_addend) {
      result = src3;
    } else if (result_is_zero) {
      result = (!is_subtract && src3_sign) ? 0x80000000 : 0x00000000;
    } else {
      result = (result_sign << 31) | ((result_exponent & 0xff) << 23) | (result_mantissa & 0x007fffff);
    }
  }
  return result;
}

unsigned riscv_fdiv_fsqrt(unsigned src1, unsigned src2, unsigned char rm, unsigned char is_sqrt, unsigned char *exception) {
  unsigned result;
  unsigned char src1_sign = fp_get_sign(src1);
  unsigned char src1_exponent = fp_get_exponent(src1);
  unsigned src1_mantissa = fp_get_mantissa(src1);
  int src1_exponent_value = (src1_exponent == 0) ? -(int)uint32_get_leadingzero(src1_mantissa) : (int)src1_exponent;
  unsigned src1_mantissa_value = (src1_exponent == 0) ? (src1_mantissa << 1) << uint32_get_leadingzero(src1_mantissa) : (0x00800000 | src1_mantissa);
  unsigned char src1_is_zero = riscv_iszero(src1);
  unsigned char src1_is_inf = (riscv_isinf(src1) == 0) ? 0 : 1;
  unsigned char src1_is_nan = (riscv_isqnan(src1) || riscv_issnan(src1)) ? 1 : 0;
  unsigned char src1_is_neg = (src1_sign && src1 != 0x80000000) ? 1 : 0;
  unsigned char src2_sign = fp_get_sign(src2);
  unsigned char src2_exponent = fp_get_exponent(src2);
  unsigned src2_mantissa = fp_get_mantissa(src2);
  int src2_exponent_value = (src2_exponent == 0) ? -(int)uint32_get_leadingzero(src2_mantissa) : (int)src2_exponent;
  unsigned src2_mantissa_value = (src2_exponent == 0) ? (src2_mantissa << 1) << uint32_get_leadingzero(src2_mantissa) : (0x00800000 | src2_mantissa);
  unsigned char src2_is_zero = riscv_iszero(src2);
  unsigned char src2_is_inf = (riscv_isinf(src2) == 0) ? 0 : 1;
  unsigned char src2_is_nan = (riscv_isqnan(src2) || riscv_issnan(src2)) ? 1 : 0;

  unsigned char result_is_nan = (is_sqrt == 0) ? (src1_is_nan || src2_is_nan || (src1_is_zero && src2_is_zero) || (src1_is_inf && src2_is_inf)) : (src1_is_nan || src1_is_neg);
  unsigned char result_is_inf = (is_sqrt == 0) ? (src1_is_inf || src2_is_zero) : (!src1_sign && src1_is_inf);
  unsigned char result_is_zero = (is_sqrt == 0) ? (src1_is_zero || src2_is_inf) : src1_is_zero;
  unsigned char result_sign = (is_sqrt == 0 && (src1_sign ^ src2_sign) != 0) ? 1 : 0;

  unsigned char dividend_normalize = (src1_mantissa_value < src2_mantissa_value) ? 1 : 0;
  int virtual_exponent = (int)src1_exponent_value - (int)src2_exponent_value + 127 - (int)dividend_normalize;
  int subnormal = (is_sqrt == 0 && virtual_exponent <= -24) ? 1 : 0;

  if (result_is_inf) {
    // Edge: 0 division
    if (is_sqrt == 0 && src2_is_zero) {
      if (exception) *exception |= FEXT_ACCURUED_EXCEPTION_DZ;
      result = (src2 & 0x80000000) ? RISCV_NINF : RISCV_PINF;
    } else {
      result = (result_sign) ? RISCV_NINF : RISCV_PINF;
    }
  } else if (result_is_nan) {
    // Edge: sqrt of negative
    if (exception) *exception |= FEXT_ACCURUED_EXCEPTION_NV;
    result = RISCV_CANONICAL_QNAN;
  } else if (result_is_zero) {
    if (is_sqrt == 0) {
      result = (result_sign) ? 0x80000000 : 0x00000000;
    } else {
      result = (src1_sign) ? 0x80000000 : 0x00000000;
    }
  } else {
    unsigned counter = (is_sqrt == 0) ? 24 : 22;
    int remainder = (is_sqrt == 0) ? (dividend_normalize ? (src1_mantissa_value << 1) : src1_mantissa_value) : ((src1_exponent_value & 0x1) ? (src1_mantissa_value << 1) - 0x01e40000 : (src1_mantissa_value << 2) - 0x02400000);
    unsigned quotient = (is_sqrt == 0) ? 0x0 : ((src1_exponent_value & 0x1) ? 0x01600000 : 0x01800000);
    for (int i = counter; i >= 0; i -= 2) {
      unsigned dividend = (is_sqrt == 0) ? ((src2_mantissa_value >> 20) & 0x07) : (((quotient >> 25) & 0x1) << 3) | ((quotient >> 21) & 0x07);
      int srt_rem = (remainder >> 21) & 0x3f;
      srt_rem = ((srt_rem & 0x20) == 0) ? srt_rem : (0xffffffc0 | srt_rem);
      int q = 0;
      int th12 = (dividend < 1) ? 6 : ((dividend < 2) ? 7: ((dividend < 4) ? 8 : ((dividend < 5) ? 9 : ((dividend < 6) ? 10 : 11))));
      int th01 = (dividend < 2) ? 2 : ((dividend < 6) ? 3 : 4);
      if (srt_rem < -th12) {
        q = -2;
      } else if (srt_rem < -th01) {
        q = -1;
      } else if (srt_rem < th01) {
        q = 0;
      } else if (srt_rem < th12) {
        q = 1;
      } else {
        q = 2;
      }
      switch (q) {
      case 2:
        if (is_sqrt == 0) {
          remainder = (int)(remainder << 2) - (int)(src2_mantissa_value << 3);
        } else {
          remainder = (int)(remainder << 2) - (int)(quotient << 2) - (int)(4 << i);
        }
        break;
      case 1:
        if (is_sqrt == 0) {
          remainder = (int)(remainder << 2) - (int)(src2_mantissa_value << 2);
        } else {
          remainder = (int)(remainder << 2) - (int)(quotient << 1) - (int)(1 << i);
        }
        break;
      case -1:
        if (is_sqrt == 0) {
          remainder = (int)(remainder << 2) + (int)(src2_mantissa_value << 2);
        } else {
          remainder = (int)(remainder << 2) + (int)(quotient << 1) - (int)(1 << i);
        }
        break;
      case -2:
        if (is_sqrt == 0) {
          remainder = (int)(remainder << 2) + (int)(src2_mantissa_value << 3);
        } else {
          remainder = (int)(remainder << 2) + (int)(quotient << 2) - (int)(4 << i);
        }
        break;
      default:
        remainder = remainder << 2;
        break;
      }
      quotient = quotient + (q << i);
    }
    unsigned long long before_round = (subnormal == 1) ? ((unsigned long long)(((0x1 << 24) | ((unsigned long long)quotient & 0x00ffffff)) << 1)) >> (unsigned)(-virtual_exponent) : (((unsigned long long)quotient & 0x00ffffff) << 24);
    unsigned char before_round_lsb = (before_round & 0x02000000) ? 1 : 0;
    unsigned char before_round_guard = (before_round & 0x01000000) ? 1 : 0;
    unsigned char before_round_sticky = ((before_round & 0x00ffffff) || (remainder != 0));
    unsigned char roundup = 0;
    switch (rm) {
    case FEXT_ROUNDING_MODE_RNE:
      roundup = before_round_guard && ((before_round_lsb && !before_round_sticky) || (before_round & 0x00ffffff) || (remainder > 0));
      break;
    case FEXT_ROUNDING_MODE_RTZ: // round to zero
      roundup = 0;
      break;
    case FEXT_ROUNDING_MODE_RDN: // round to negative inf
      roundup = ((before_round_guard | before_round_sticky) & result_sign) ? 1 : 0;
      break;
    case FEXT_ROUNDING_MODE_RUP: // round to positive inf
      roundup = ((before_round_guard | before_round_sticky) & (!result_sign)) ? 1 : 0;
      break;
    case FEXT_ROUNDING_MODE_RMM: // round to magnitude (large abs)
      roundup = before_round_guard;
      break;
    default:
      roundup = 0;
      break;
    }
    unsigned char expinc = (roundup && (((before_round >> 25) & 0x007fffff) == 0x007fffff)) ? 1 : 0;
    unsigned result_mantissa = (((before_round >> 25) & 0x007fffff) + roundup) & 0x007fffff;
    int result_exponent = (is_sqrt == 0) ? (subnormal ? 0 : virtual_exponent + expinc) : (src1_exponent_value >> 1) + (src1_exponent & 0x1) + 63;
    result_is_inf |= ((is_sqrt == 0) ? ((virtual_exponent > 255) || (result_exponent == 0xff)) : result_is_inf);
    if (result_is_inf) {
      result = ((result_sign << 31) & 0x80000000) | ((0xff << 23) & 0x7f800000);
      if (exception && (before_round_guard || before_round_sticky)) {
        *exception = FEXT_ACCURUED_EXCEPTION_NV;
      }
    } else {
      result = ((result_sign << 31) & 0x80000000) | ((result_exponent << 23) & 0x7f800000) | (result_mantissa & 0x007fffff);
      if (exception && (before_round_guard || before_round_sticky)) {
        *exception = FEXT_ACCURUED_EXCEPTION_NX;
      }
    }
  }
  return result;
}

unsigned riscv_fmin_fmax(unsigned src1, unsigned src2, unsigned char is_fmax, unsigned char *exception) {
  unsigned ret = 0;
  union { unsigned i; float f; } s1, s2;
  s1.i = src1;
  s2.i = src2;
  if (riscv_isnan(src1) && riscv_isnan(src2)) {
    ret = RISCV_CANONICAL_QNAN;
  } else if (riscv_isnan(src1)) {
    ret = src2;
  } else if (riscv_isnan(src2)) {
    ret = src1;
  } else if (src1 == 0x00000000 && src2 == 0x80000000) {
    ret = (is_fmax == 0) ? src2 : src1;
  } else if (src1 == 0x80000000 && src2 == 0x00000000) {
    ret = (is_fmax == 0) ? src1 : src2;
  } else if (is_fmax == 0) {
    ret = (s1.f < s2.f) ? src1 : src2;
  } else {
    ret = (s1.f > s2.f) ? src1 : src2;
  }
  if (riscv_issnan(src1) || riscv_issnan(src2)) {
    *exception |= FEXT_ACCURUED_EXCEPTION_NV;
  }
  return ret;
}

unsigned riscv_fle(unsigned src1, unsigned src2, unsigned char *exception) {
  unsigned ret = 0;
  union { unsigned i; float f; } s1, s2;
  s1.i = src1;
  s2.i = src2;
  if (riscv_isnan(src1) || riscv_isnan(src2)) {
    ret = 0;
    *exception |= FEXT_ACCURUED_EXCEPTION_NV;
  } else {
    ret = (s1.f <= s2.f) ? 1 : 0;
  }
  return ret;
}

unsigned riscv_flt(unsigned src1, unsigned src2, unsigned char *exception) {
  unsigned ret = 0;
  union { unsigned i; float f; } s1, s2;
  s1.i = src1;
  s2.i = src2;
  if (riscv_isnan(src1) || riscv_isnan(src2)) {
    ret = 0;
    *exception |= FEXT_ACCURUED_EXCEPTION_NV;
  } else {
    ret = (s1.f < s2.f) ? 1 : 0;
  }
  return ret;
}

unsigned riscv_feq(unsigned src1, unsigned src2, unsigned char *exception) {
  unsigned ret = 0;
  union { unsigned i; float f; } s1, s2;
  s1.i = src1;
  s2.i = src2;
  if (riscv_issnan(src1) || riscv_issnan(src2)) {
    ret = 0;
    *exception |= FEXT_ACCURUED_EXCEPTION_NV;
  } else if (riscv_isnan(src1) || riscv_isnan(src2)) {
    ret = 0;
  } else {
    ret = (s1.f == s2.f) ? 1 : 0;
  }
  return ret;
}

unsigned riscv_fcvtws(unsigned src1, unsigned char rm, unsigned char is_unsigned, unsigned char *exception) {
  unsigned result = 0;
  unsigned char sign = fp_get_sign(src1);
  unsigned char exponent = fp_get_exponent(src1);
  unsigned mantissa = fp_get_mantissa(src1);
  unsigned mantissa_value = (0x00800000 | mantissa) << 8; // shadow + 8 shift
  unsigned char src1_is_neg = sign;
  unsigned char src1_is_nan = riscv_isqnan(src1) || riscv_issnan(src1);
  // exponent 126 (e= -1) -> zero
  // exponent 127 (e=  0) -> shift >> 23 (= mantissa len)       [31 shift for mantissa_value]
  // exponont 150 (e= 23) -> shift 0                            [ 8 shift for mantissa_value]
  // exponont 158 (e= 31) -> shift << 8 (if signed -> overflow) [ 0 shift for mantissa_value]
  // exponont 159 (e= 32) -> inf (including shadow 1 bit)
  int shift = (127 + 31) - exponent;
  unsigned long long mantissa_value_shifted = ((unsigned long long)mantissa_value << 32) >> shift;
  unsigned cvt_integer_part = 0;
  if (shift == 0) {
    if (is_unsigned) {
      cvt_integer_part = (src1_is_neg) ? 0x00000000 : (mantissa_value_shifted >> 32) & 0xffffffff;
    } else {
      cvt_integer_part = (src1_is_neg) ? 0x80000000 : 0x7fffffff; // INT_MAX
    }
  } else if (shift < 0) {
    if (is_unsigned) {
      cvt_integer_part = (!src1_is_nan && src1_is_neg) ? 0x00000000 : 0xffffffff; // UNSIGNED INT_MAX
    } else {
      cvt_integer_part = (!src1_is_nan && src1_is_neg) ? 0x80000000 : 0x7fffffff; // INT_MAX
    }
  } else {
    cvt_integer_part = (mantissa_value_shifted >> 32) & 0xffffffff;
  }
  unsigned char roundup = riscv_rounding(mantissa_value_shifted, src1_is_neg, rm);
  if (shift < 1) {
    result = cvt_integer_part;
  } else {
    result = cvt_integer_part + roundup;
    if (src1_is_neg && is_unsigned) {
      result = 0;
    } else if (src1_is_neg && !is_unsigned) {
      result = -result;
    }
  }
  if (exception) {
    *exception = 0;
    if ((shift < 0) || // overflow or either src1 is nan or inf
        (!is_unsigned && shift == 0 && (!src1_is_neg || mantissa != 0)) || // rounding result is overflow (signed conversion)
        (is_unsigned && src1_is_neg && (shift <= 31 || roundup)) // rounding result is negative (unsiged conversion)
        ) {
      *exception |= FEXT_ACCURUED_EXCEPTION_NV;
    } else if (riscv_inexact(mantissa_value_shifted)) {
      *exception |= FEXT_ACCURUED_EXCEPTION_NX;
    }
  }
  return result;
}

unsigned riscv_fcvtsw(unsigned src1, unsigned char rm, unsigned char is_unsigned, unsigned char *exception) {
  unsigned result = 0;
  unsigned char src1_is_neg = (!is_unsigned && (0x80000000 & src1)) ? 1 : 0;
  unsigned src1_abs = (src1_is_neg) ? (~src1 + 1) : src1;
  unsigned leading_zero = uint32_get_leadingzero(src1_abs);
  unsigned src1_shifted = (leading_zero == 32) ? 0 : (src1_abs << leading_zero);
  // extract 23bit (the 24th bit is shadow bit)
  unsigned mantissa = (src1_shifted >> 8) & (0x007fffff);
  unsigned long long mantissa_long = ((unsigned long long)mantissa << 32) >> 8;
  // leading_zero = 32 (i.e. 0x00000000) ... src1 = 0  (expo =   0)
  // leading_zero = 31 (i.e. 0x00000001) ... e =  0    (expo = 127)
  // leading_zero = 0  (i.e. 0x80000000) ... e = 31    (expo = 158)
  unsigned char exponent = (src1 == 0) ? 0 : (31 - leading_zero) + 127;
  unsigned char roundup = riscv_rounding(mantissa_long, src1_is_neg, rm);
  unsigned char expinc = (roundup && mantissa == 0x007fffff) ? 1 : 0;

  exponent = exponent + expinc;
  mantissa = (mantissa + roundup) & 0x007fffff;
  result = (src1_is_neg << 31) | (exponent << 23) | mantissa;
  if (exception && riscv_inexact(mantissa_long)) {
    *exception = FEXT_ACCURUED_EXCEPTION_NX;
  }
  return result;
}

unsigned riscv_fclass(unsigned src1) {
  union { unsigned i; float f; } s = { src1 };
  unsigned ret = 0;
  if (riscv_isinf(src1) == -1) ret |= 0x00000001;
  if (isnormal(s.f) && (src1 & 0x80000000)) ret |= 0x00000002;
  if ((fpclassify(s.f) == FP_SUBNORMAL) && (src1 & 0x80000000)) ret |= 0x00000004;
  if ((fpclassify(s.f) == FP_ZERO) && (src1 & 0x80000000)) ret |= 0x00000008;
  if ((fpclassify(s.f) == FP_ZERO) && !(src1 & 0x80000000)) ret |= 0x00000010;
  if ((fpclassify(s.f) == FP_SUBNORMAL) && !(src1 & 0x80000000)) ret |= 0x00000020;
  if (isnormal(s.f) && !(src1 & 0x80000000)) ret |= 0x00000040;
  if (riscv_isinf(src1) == 1) ret |= 0x00000080;
  if (riscv_issnan(src1)) ret |= 0x00000100; // signaling NaN
  if (riscv_isqnan(src1)) ret |= 0x00000200; // quiet NaN
  return ret;
}

unsigned riscv_get_opcode(unsigned inst) { return inst & 0x0000007f; }
unsigned riscv_get_rs1(unsigned inst) { return (inst >> 15) & 0x0000001f; }
unsigned riscv_get_rs2(unsigned inst) { return (inst >> 20) & 0x0000001f; }
unsigned riscv_get_rs3(unsigned inst) { return (inst >> 27) & 0x0000001f; }
unsigned riscv_get_rd(unsigned inst) { return (inst >> 7) & 0x0000001f; }
unsigned riscv_get_funct3(unsigned inst) { return (inst >> 12) & 0x00000007; }
unsigned riscv_get_funct5(unsigned inst) { return (inst >> 27) & 0x0000001f; }
unsigned riscv_get_funct7(unsigned inst) { return (inst >> 25) & 0x0000007f; }
unsigned riscv_get_funct12(unsigned inst) { return (inst >> 20) & 0x00000fff; }
unsigned riscv_get_branch_offset(unsigned inst) {
  return ((((int)inst >> 19) & 0xfffff000) |
          (((inst >> 25) << 5) & 0x000007e0) |
          (((inst >> 7) & 0x00000001) << 11) | ((inst >> 7) & 0x0000001e));
}
unsigned riscv_get_jalr_offset(unsigned inst) { return ((int)inst >> 20); }
unsigned riscv_get_jal_offset(unsigned inst) {
  return ((((int)inst >> 11) & 0xfff00000) | (inst & 0x000ff000) |
          (((inst >> 20) & 0x00000001) << 11) |
          ((inst >> 20) & 0x000007fe));
}
unsigned riscv_get_store_offset(unsigned inst) {
  return ((((int)inst >> 25) << 5) | ((inst >> 7) & 0x0000001f));
}
unsigned riscv_get_load_offset(unsigned inst) { return ((int)inst >> 20); }
unsigned riscv_get_csr_addr(unsigned inst) { return ((inst >> 20) & 0x00000fff); }
unsigned riscv_get_csr_imm(unsigned inst) { return (inst >> 15) & 0x0000001f; }
unsigned riscv_get_immediate(unsigned inst) { return ((int)inst >> 20); }
unsigned char riscv_get_rm(unsigned inst) { return (inst >> 12) & 0x00000007; }
