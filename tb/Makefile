PACKAGES=ladybird_config.svh ladybird_riscv_helper.svh
INTERFACE=ladybird_axi_interface
TOP=ladybird_tb
OBJDIR=build
ELF?=./hello.riscv
VDEF=-DLADYBIRD_SIMULATION
TIMEOUT?=2147483647

.PHONY: all clean

all:
	verilator -Wall $(VDEF) --timing --cc --exe $(TOP).cpp -I../src $(PACKAGES) $(INTERFACE) --trace --top-module $(TOP) $(TOP).sv
	cd obj_dir; make -j --silent -f V$(TOP).mk V$(TOP)
	cd obj_dir; ./V$(TOP) ../$(ELF) $(TIMEOUT)

clean:
	$(RM) -rf obj_dir
