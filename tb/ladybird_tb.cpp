#include "verilated.h"
#include "Vladybird_tb.h"

#define DEFAULT_ELF_PATH "hello.riscv"

int main(int argc, char **argv, char **) {


  // Setup context, defaults, and parse command line
  Verilated::debug(0);
  const std::unique_ptr<VerilatedContext> contextp{new VerilatedContext};
  contextp->traceEverOn(true);
  contextp->commandArgs(argc, argv);

  // Construct the Verilated model, from Vtop.h generated from Verilating
  const std::unique_ptr<Vladybird_tb> topp{new Vladybird_tb{contextp.get()}};

  // Set ELF path for simulation
  if (argc > 1) {
    topp->ELF_PATH = argv[1];
  } else {
    topp->ELF_PATH = DEFAULT_ELF_PATH;
  }

  // Simulate until $finish
  while (!contextp->gotFinish()) {
    // Evaluate model
    topp->eval();
    // Advance time
    contextp->timeInc(1);
  }

  if (!contextp->gotFinish()) {
    VL_DEBUG_IF(VL_PRINTF("+ Exiting without $finish; no events left\n"););
  }

  // Final model cleanup
  topp->final();
  return 0;
}
