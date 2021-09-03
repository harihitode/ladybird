`timescale 1 ns / 1 ps

module tb_ladybird;
  import ladybird_config::*;

  function automatic logic [19:0] HI(input logic [31:0] immediate);
    return immediate[31:12];
  endfunction

  function automatic logic [11:0] LO(input logic [31:0] immediate);
    return immediate[11:0];
  endfunction

  function automatic logic [31:0] ADDI(input logic [4:0] rd, input logic [4:0] rs, input logic [11:0] immediate);
    return {immediate, rs, 3'b000, rd, 7'b0010011};
  endfunction

  function automatic logic [31:0] LUI(input logic [4:0] rd, input logic [19:0] immediate);
    return {immediate, rd, 7'b0110111};
  endfunction

  function automatic logic [31:0] LW(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset, rt, 3'b010, rd, 7'b0000011};
  endfunction

  function automatic logic [31:0] SW(input logic [4:0] rd, input logic [11:0] offset, input logic [4:0] rt);
    return {offset[11:5], rd, rt, 3'b010, offset[4:0], 7'b0100011};
  endfunction

  function automatic logic [31:0] JAL(input logic [4:0] rd, input logic [20:0] offset);
    return {offset[20], offset[10:1], offset[11], offset[19:12], rd, 7'b1101111};
  endfunction

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
                           LUI(5'd1, HI(32'hFFFF_FFFF)),
                           LW(5'd2, 12'h000, 5'd1),
                           ADDI(5'd2, 5'd2, 12'h001),
                           SW(5'd2, 12'h000, 5'd1),
                           JAL(5'd1, -21'd12)
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
