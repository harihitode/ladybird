`timescale 1 ns / 1 ps
module ladybird_crossbar
  import ladybird_config::*;
  #(
    parameter N_CORE_BUS = 2,
    parameter N_PERIPHERAL_BUS = 4
    )
  (
   input logic clk,
   ladybird_bus_interface.secondary core_ports [N_CORE_BUS],
   ladybird_bus_interface.primary peripheral_ports [N_PERIPHERAL_BUS],
   input logic nrst
   );

  typedef struct packed {
    logic        valid;
    logic        requested;
    logic        filled;
    logic [31:0] addr;
    logic [31:0] data;
    logic [3:0]  wstrb;
  } request_t;

  request_t [N_CORE_BUS-1:0] requests;
  logic [N_CORE_BUS-1:0]     requests_gnt;
  logic [N_CORE_BUS-1:0]     requests_data_gnt;
  logic [N_CORE_BUS-1:0]     core_ports_load_completed;;
  logic [N_CORE_BUS-1:0]     core_ports_store_completed;
  access_t [N_CORE_BUS-1:0]  core_ports_access;

  logic [N_PERIPHERAL_BUS-1:0] p_gnt;
  logic [N_PERIPHERAL_BUS-1:0] p_data_gnt;
  logic [N_PERIPHERAL_BUS-1:0][31:0] p_data;

  generate for (genvar i = 0; i < N_PERIPHERAL_BUS; i++) begin
    assign p_gnt[i] = peripheral_ports[i].gnt;
    assign p_data_gnt[i] = peripheral_ports[i].rdgnt;
    assign p_data[i] = peripheral_ports[i].rdata;
  end endgenerate

  generate for (genvar i = 0; i < N_CORE_BUS; i++) begin
    always_comb begin
      if (requests[i].valid & |(requests[i].wstrb)) begin
        // store
        core_ports_load_completed[i] = '0;
        core_ports_store_completed[i] = p_gnt[core_ports_access[i]];
      end else if (requests[i].valid) begin
        // load
        core_ports_load_completed[i] = requests[i].filled;
        core_ports_store_completed[i] = '0;
      end else begin
        core_ports_load_completed[i] = '0;
        core_ports_store_completed[i] = '0;
      end
      requests_gnt[i] = requests[i].valid & p_gnt[core_ports_access[i]];
      requests_data_gnt[i] = requests[i].requested & p_data_gnt[core_ports_access[i]];
    end

    always_ff @(posedge clk) begin
      if (~nrst) begin
        requests[i] <= '0;
      end else begin
        if (core_ports[i].req & ~requests[i].valid) begin
          requests[i].valid <= 'b1;
          requests[i].addr <= core_ports[i].addr;
          requests[i].data <= core_ports[i].wdata;
          requests[i].wstrb <= core_ports[i].wstrb;
        end else if (core_ports_store_completed[i] | core_ports_load_completed[i])begin
          requests[i] <= '0;
        end else if (~requests[i].requested & requests_gnt[i])begin
          requests[i].requested <= 'b1;
        end else if (requests_data_gnt[i]) begin
          requests[i].filled <= 'b1;
          requests[i].data <= p_data[core_ports_access[i]];
        end
      end
    end
  end endgenerate

  generate for (genvar i = 0; i < N_CORE_BUS; i++) begin
    assign core_ports_access[i] = ACCESS_TYPE(requests[i].addr);
    assign core_ports[i].gnt = ~requests[i].valid;
    assign core_ports[i].rdgnt = requests[i].valid & requests[i].filled;
    assign core_ports[i].rdata = requests[i].data;
  end endgenerate

  logic [N_PERIPHERAL_BUS-1:0] peripheral_ports_valid;
  logic [N_PERIPHERAL_BUS-1:0] peripheral_ports_requested;
  logic [N_PERIPHERAL_BUS-1:0][31:0] peripheral_ports_addr;
  logic [N_PERIPHERAL_BUS-1:0][3:0] peripheral_ports_wstrb;
  logic [N_PERIPHERAL_BUS-1:0][31:0] peripheral_ports_data;
  generate for (genvar i = 0; i < N_PERIPHERAL_BUS; i++) begin
    always_comb begin
      peripheral_ports_valid[i] = '0;
      peripheral_ports_requested[i] = '0;
      peripheral_ports_addr[i] = '0;
      peripheral_ports_wstrb[i] = '0;
      peripheral_ports_data[i] = '0;
      for (int j = 0; j < N_CORE_BUS; j++) begin
        if (requests[j].valid && (i == core_ports_access[j])) begin
          peripheral_ports_valid[i] = requests[j].valid;
          peripheral_ports_requested[i] = requests[j].requested;
          peripheral_ports_addr[i] = requests[j].addr;
          peripheral_ports_wstrb[i] = requests[j].wstrb;
          peripheral_ports_data[i] = requests[j].data;
          break;
        end
      end
    end

    assign peripheral_ports[i].req = peripheral_ports_valid[i] & ~peripheral_ports_requested[i];
    assign peripheral_ports[i].addr = peripheral_ports_addr[i];
    assign peripheral_ports[i].wstrb = peripheral_ports_wstrb[i];
    assign peripheral_ports[i].wdata = peripheral_ports_data[i];
  end endgenerate
endmodule
