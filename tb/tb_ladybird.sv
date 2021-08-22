`timescale 1 ns / 1 ps

module tb_ladybird;
  import ladybird_config::*;
  logic clk = '0;
  logic anrst = '0;
  logic anrst_c = '0;
  logic nrst = 'b1;
  logic uart_txd_in = 'b1;
  logic uart_rxd_out;
  logic [3:0] btn = '0;
  logic [3:0] led;

  ladybird_bus inst_ram_writer();
  ladybird_bus core_ibus();
  ladybird_bus inst_bus();

  logic [31:0] iram [] = '{
                           32'h0a0a_0a0a,
                           32'hbeaf_cafe,
                           32'hcccc_cccc,
                           32'h8888_8888,
                           32'h0b0b_0b0b,
                           32'hcafe_beaf,
                           32'h2222_2222,
                           32'h7777_7777
                           };
  logic [31:0] iram_data;
  assign inst_ram_writer.primary.data = iram_data;
  task write_instruction ();
    for (int i = 0; i < $size(iram); i++) begin
      @(negedge clk);
      inst_ram_writer.primary.req = 'b1;
      inst_ram_writer.primary.addr = (i << 2);
      inst_ram_writer.primary.wstrb = '1;
      iram_data = iram[i];
      @(posedge clk);
      wait (inst_ram_writer.req && inst_ram_writer.gnt);
    end
    inst_ram_writer.primary.req = 'b0;
    $display("instruction has been written.");
  endtask

  initial forever #5 clk = ~clk;

  initial begin
    #100 anrst = 'b1;
    write_instruction();
    #10 anrst_c = 'b1;
  end

  ladybird_top #(.SIMULATION(1))
  DUT (
       .*,
       .inst_bus(core_ibus),
       .anrst(anrst_c)
       );

  ladybird_bus_arbitrator_beh #(.N_INPUT(2))
  IBUS_ARB
    (
     .clk(clk),
     .in_bus('{inst_ram_writer, core_ibus}),
     .out_bus(inst_bus),
     .nrst(nrst),
     .anrst(anrst)
     );

  ladybird_ram_beh #(.DATA_W(XLEN), .ADDR_W(3))
  IRAM
    (
     .clk(clk),
     .bus(inst_bus),
     .nrst(nrst),
     .anrst(anrst)
     );

endmodule
