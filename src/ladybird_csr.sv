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
    logic        SD;
    logic [7:0]  reserved1;
    logic        TSR;
    logic        TW;
    logic        TVM;
    logic        MXR;
    logic        SUM;
    logic        MPRV;
    logic [1:0]  XS;
    logic [1:0]  FS;
    logic [1:0]  MPP;
    logic [1:0]  VS;
    logic        SPP;
    logic        MPIE;
    logic        UBE;
    logic        SPIE;
    logic        reserved2;
    logic        MIE;
    logic        reserved3;
    logic        SIE;
    logic        reserved4;
  } status_t;

  typedef struct packed {
    logic [25:0] reserved1;
    logic        MBE;
    logic        SBE;
    logic [3:0]  reserved2;
  } statush_t;

  typedef struct packed {
    logic [1:0]  xml;
    logic [3:0]  reserved;
    logic [25:0] EXTENSION;
  } isa_t;

  logic [XLEN-1:0]        masked;
  logic [XLEN-1:0]        m_tvec;
  // verilator lint_off UNUSEDSIGNAL
  status_t                m_status, status_d;
  statush_t               m_statush, statush_d;
  // verilator lint_on UNUSEDSIGNAL
  isa_t                   m_isa;

  // Status Signals
  logic                   status_dirty;
  logic [1:0]             f_status;
  logic [1:0]             x_status;
  logic [1:0]             m_previous_mode;
  logic                   s_access_usermemory;
  logic                   s_previous_mode;
  logic                   m_previous_interrupt_en;
  logic                   s_previous_interrupt_en;
  logic                   m_current_interrupt_en;
  logic                   s_current_interrupt_en;
  logic [1:0]             current_mode;

  assign f_status = '0;
  assign x_status = '0;
  assign status_dirty = &f_status | &x_status;
  // F-Extension
  always_comb begin
    m_status.SD = status_dirty;
    m_status.reserved1 = '0;
    m_status.TSR = '0; // trap sret
    m_status.TW = '0; // timeout for wfi
    m_status.TVM = '0; // trap virtual memory access
    m_status.MXR = '0; // make executable readable
    m_status.SUM = s_access_usermemory;
    m_status.MPRV = '0; // modify privilege
    m_status.XS = x_status;
    m_status.FS = f_status;
    m_status.MPP = m_previous_mode;
    m_status.VS = '0;
    m_status.SPP = s_previous_mode;
    m_status.MPIE = m_previous_interrupt_en;
    m_status.UBE = '0;
    m_status.SPIE = s_previous_interrupt_en;
    m_status.reserved2 = '0;
    m_status.MIE = m_current_interrupt_en;
    m_status.reserved3 = '0;
    m_status.SIE = s_current_interrupt_en;
    m_status.reserved4 = '0;
    m_statush = '0;
  end

  assign status_d = masked;

  always_ff @(posedge clk) begin
    if (~nrst) begin
      current_mode <= PRIV_MODE_M;
      s_access_usermemory <= '0;
      m_previous_mode <= PRIV_MODE_M;
      s_previous_mode <= PRIV_MODE_S[0];
      m_previous_interrupt_en <= '0;
      s_previous_interrupt_en <= '0;
      m_current_interrupt_en <= '0;
      s_current_interrupt_en <= '0;
    end else begin
      if (i_valid && i_addr == CSR_ADDR_M_STATUS) begin
        s_access_usermemory <= status_d.SUM;
      end
      if (i_valid && i_addr == CSR_ADDR_M_STATUS) begin
        m_previous_mode <= status_d.MPP;
      end
      if (i_valid && i_addr == CSR_ADDR_M_STATUS) begin
        s_previous_mode <= status_d.SPP;
      end
      if (i_valid && i_addr == CSR_ADDR_M_STATUS) begin
        m_previous_interrupt_en <= status_d.MPIE;
      end
      if (i_valid && i_addr == CSR_ADDR_M_STATUS) begin
        s_previous_interrupt_en <= status_d.SPIE;
      end
      if (i_valid && i_addr == CSR_ADDR_M_STATUS) begin
        m_previous_interrupt_en <= status_d.MPIE;
      end
      if (i_valid && i_addr == CSR_ADDR_M_STATUS) begin
        s_previous_interrupt_en <= status_d.SPIE;
      end
    end
  end

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
      CSR_ADDR_M_STATUSH: o_data = m_statush;
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

  `LADYBIRD_SIMPLE_CSR(CSR_ADDR_M_TVEC, m_tvec, 'd0)

`ifdef LADYBIRD_SIMULATION
  always_ff @(posedge clk) begin
    if (nrst & i_valid & unimpl) begin
      $display("hart%0d mode%0d [%s] unimpl. addr %08x data %08x", HART_ID, current_mode, operation, i_addr, i_data);
    end
  end
`endif

endmodule
