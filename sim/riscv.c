#include "riscv.h"
#include <stdio.h>
#include <string.h>

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
  case OPCODE_AUIPC:
    sprintf(buf, "AUIPC");
    break;
  case OPCODE_LUI:
    sprintf(buf, "LUI");
    break;
  case OPCODE_JALR:
    sprintf(buf, "JALR");
    break;
  case OPCODE_JAL:
    sprintf(buf, "JAL");
    break;
  case OPCODE_STORE: {
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
  }
  case OPCODE_LOAD: {
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
  }
  case OPCODE_AMO: {
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
  }
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
  case OPCODE_BRANCH: {
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
  }
  default:
    break;
  }
  return buf;
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
unsigned riscv_get_jalr_offset(unsigned inst) {
  return ((int)inst >> 20);
}
unsigned riscv_get_jal_offset(unsigned inst) {
  return ((((int)inst >> 11) & 0xfff00000) | (inst & 0x000ff000) |
          (((inst >> 20) & 0x00000001) << 11) |
          ((inst >> 20) & 0x000007fe));
}
unsigned riscv_get_store_offset(unsigned inst) {
  return ((((int)inst >> 25) << 5) | ((inst >> 7) & 0x0000001f));
}
unsigned riscv_get_load_offset(unsigned inst) {
  return ((int)inst >> 20);
}
unsigned riscv_get_csr_addr(unsigned inst) {
  return ((inst >> 20) & 0x00000fff);
}
unsigned riscv_get_csr_imm(unsigned inst) { return (inst >> 15) & 0x0000001f; }
unsigned riscv_get_immediate(unsigned inst) {
  return ((int)inst >> 20);
}
