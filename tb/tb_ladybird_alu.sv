`timescale 1 ns / 1 ps

module tb_ladybird_alu;
  logic clk = '0;
  initial forever #5 clk = ~clk;

  logic [2:0] operation = 3'b010; // ADD

  logic [31:0] src1, src2, q_ref, q;

  typedef struct packed
                 {
                   logic [31:0] src1;
                   logic [31:0] src2;
                   logic [31:0] q_ref;
                 } vector_t;

  logic [0:3][31:0]             vector [10] = '{
                                                {32'h66666667, 32'h66666667, 32'd0},
                                                {32'h33333334, 32'h33333334, 32'd0},
                                                {-32'h00004001, -32'h00004001, 32'd0},
                                                {-32'h00000201, 32'h00000005, 32'd1},
                                                {32'h33333334, -32'h80000000, 32'd0},
                                                {32'h08000000, 32'h00000000, 32'd0},
                                                {32'h00020000, 32'h7fffffff, 32'd1},
                                                {-32'h00020001, 32'h1, 32'd1},
                                                {-32'h80000000, 32'd400, 32'd1},
                                                {32'h00000000, 32'h00000008, 32'd1}
                                                };

  ladybird_alu #(.SIMULATION(1))
  DUT
    (
     .OPERATION(operation),
     .ALTERNATE('b0),
     .SRC1(src1),
     .SRC2(src2),
     .Q(q)
     );

  initial begin
    automatic vector_t current_vector;
    for (int i = 0; i < $size(vector); i++) begin
      @(negedge clk);
      current_vector = vector[i];
      src1 = current_vector.src1;
      src2 = current_vector.src2;
      q_ref = current_vector.q_ref;
      @(posedge clk);
      $display("%08x, %08x, %08x, %08x", src1, src2, q_ref, q);
      if (q_ref != q) begin
        $display("FAILURE");
      end
    end
    $finish;
  end

endmodule
