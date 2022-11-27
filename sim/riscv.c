#include "riscv.h"
#include <stdio.h>

const char *get_mnemonic(unsigned inst) {
  static char buf[128];
  sprintf(buf, "ILLEGAL");
  unsigned funct3 = (inst >> 12) & 0x00000007;
  unsigned funct5 = (inst >> 27) & 0x0000001f;
  unsigned funct7 = (inst >> 25) & 0x0000007f;
  unsigned rs2 = (inst >> 20) & 0x0000001f;
  unsigned alternate = (inst & 0x40000000);
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
    break;
  }
  case OPCODE_MISC_MEM:
    sprintf(buf, "FENCE");
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
