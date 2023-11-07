`timescale 1 ns / 1 ps

`include "../src/ladybird_config.svh"
`include "../src/ladybird_riscv_helper.svh"

`define LADYBIRD_SIMPLE_CSR(ADDR, SIGNAL, DEFVAL) \
always_ff @(posedge clk) begin \
  if (~nrst) begin \
    SIGNAL <= DEFVAL; \
  end else begin \
    if (i_valid & i_addr == ADDR) begin \
      SIGNAL <= masked; \
    end \
  end \
end

module ladybird_csr
  import ladybird_config::*;
  import ladybird_riscv_helper::*;
#(parameter logic [XLEN-1:0] HART_ID = 'd0)
(
  input logic             clk,
  input logic [63:0]      rtc,
  input logic             retire,
// verilator lint_off UNUSED
  input logic [XLEN-1:0]  retire_inst,
  input logic [XLEN-1:0]  retire_pc,
  input logic [XLEN-1:0]  retire_next_pc,
// verilator lint_on UNUSED
  input logic [2:0]       i_op,
  input logic             i_valid,
  input logic [11:0]      i_addr,
  input logic [XLEN-1:0]  i_data,
  output logic [XLEN-1:0] o_data,
  input logic             nrst
);

  logic [63:0]            minstret, mcycle;
`ifdef LADYBIRD_SIMULATION_DEBUG_DUMP
  string                  inst_disas;
  always_comb begin
    inst_disas = ladybird_riscv_helper::riscv_disas(retire_inst);
  end
`endif
  always_ff @(posedge clk) begin
    if (~nrst) begin
      minstret <= '0;
      mcycle <= '0;
    end else begin
      mcycle <= mcycle + 'd1;
      if (retire) begin
        minstret <= minstret + 'd1;
`ifdef LADYBIRD_SIMULATION_DEBUG_DUMP
        $display("%0d %08x, %08x, %s", rtc, retire_pc, retire_inst, inst_disas);
`endif
      end
    end
  end

  typedef struct packed {
    logic [12:0] reserved1;
    logic        SUM;
    logic [4:0]  reserved2;
    logic [1:0]  MPP;
    logic [1:0]  reserved3;
    logic        SPP;
    logic        MPIE;
    logic        reserved4;
    logic        SPIE;
    logic        reserved5;
    logic        MIE;
    logic        reserved6;
    logic        SIE;
    logic        reserved7;
  } status_t;

  typedef struct packed {
    logic [1:0]  xml;
    logic [3:0]  reserved;
    logic [25:0] EXTENSION;
  } isa_t;

  logic [XLEN-1:0]        masked;
  logic [XLEN-1:0]        m_tvec;
  status_t                m_status;
  isa_t                   m_isa;

`ifdef LADYBIRD_SIMULATION
  logic                   unimpl;
`endif

  localparam logic [4:0] EXT_M = {"M" - "A"}[4:0];
  always_comb begin
    m_isa = '0;
    if (XLEN == 32) begin
      m_isa.xml = 'd1;
    end else if (XLEN == 64) begin
      m_isa.xml = 'd2;
    end else begin
      m_isa.xml = 'd3;
    end
    m_isa.EXTENSION[EXT_M] = '1; // enable M_EXTENSION
  end

  always_comb begin
    case (i_op)
      FUNCT3_CSRRS, FUNCT3_CSRRSI: masked = o_data | i_data;
      FUNCT3_CSRRC, FUNCT3_CSRRCI: masked = o_data & (~i_data);
      default: masked = i_data;
    endcase
  end

`ifdef LADYBIRD_SIMULATION
  string                  operation;
  always_comb begin
    case (i_op)
      FUNCT3_CSRRW: operation = "CSRRW";
      FUNCT3_CSRRS: operation = "CSRRS";
      FUNCT3_CSRRC: operation = "CSRRC";
      FUNCT3_CSRRWI: operation = "CSRRWI";
      FUNCT3_CSRRSI: operation = "CSRRSI";
      FUNCT3_CSRRCI: operation = "CSRRCI";
      default: operation = "UNKNOWN";
    endcase
  end
`endif

  always_comb begin
`ifdef LADYBIRD_SIMULATION
    unimpl = '0;
`endif
    case (i_addr)
      CSR_ADDR_TIME: o_data = rtc[31:0];
      CSR_ADDR_CYCLE: o_data = mcycle[31:0];
      CSR_ADDR_INSTRET: o_data = minstret[31:0];
      CSR_ADDR_TIMEH: o_data = rtc[63:32];
      CSR_ADDR_CYCLEH: o_data = mcycle[63:32];
      CSR_ADDR_INSTRETH: o_data = minstret[63:32];
      CSR_ADDR_M_STATUS: o_data = m_status;
      CSR_ADDR_M_TVEC: o_data = m_tvec;
      CSR_ADDR_M_HARTID: o_data = HART_ID;
      CSR_ADDR_M_ISA: o_data = m_isa;
      default: begin
        o_data = '0;
`ifdef LADYBIRD_SIMULATION
        unimpl = '1;
`endif
      end
    endcase
  end

  `LADYBIRD_SIMPLE_CSR(CSR_ADDR_M_STATUS, m_status, 'd0)
  `LADYBIRD_SIMPLE_CSR(CSR_ADDR_M_TVEC, m_tvec, 'd0)

`ifdef LADYBIRD_SIMULATION
  always_ff @(posedge clk) begin
    if (nrst & i_valid & unimpl) begin
      $display("hart%0d [%s] unimpl. addr %08x data %08x", HART_ID, operation, i_addr, i_data);
    end
  end
`endif

endmodule
