`timescale 1 ns / 1 ps

module ladybird_full_adder
  (
   input logic  x,
   input logic  y,
   input logic  c_in,
   output logic q,
   output logic c_out
   );

  // | x | y | c_in (carry in) || q (output) | c_out (carry_out) |
  // |---|---|-----------------||------------|-------------------|
  // | 0 | 0 | 0               || 0          | 0                 |
  // | 0 | 0 | 1               || 1          | 0                 |
  // | 0 | 1 | 0               || 1          | 0                 |
  // | 0 | 1 | 1               || 0          | 1                 |
  // | 1 | 0 | 0               || 1          | 0                 |
  // | 1 | 0 | 1               || 0          | 1                 |
  // | 1 | 1 | 0               || 0          | 1                 |
  // | 1 | 1 | 1               || 1          | 1                 |

  always_comb begin: full_adder_logic
    q = (~x & ~y & c_in) | (~x & y & ~c_in) | (x & ~y & ~c_in) | (x & y & c_in);
    c_out = (~x & y & c_in) | (x & ~y & c_in) | (x & y & ~c_in) | (x & y & c_in);
  end
endmodule
