PACKAGES=ladybird_config.svh ladybird_riscv_helper.svh
INTERFACE=
TOP=ladybird_tb
OBJDIR=build
ELF=./hello.riscv

.PHONY: all clean

all:
	verilator -Wall -DLADYBIRD_SIMULATION --cc --exe $(TOP).cpp -I../src $(PACKAGES) $(INTERFACE) --trace --top-module $(TOP) $(TOP).sv
	cd obj_dir; make -j --silent -f V$(TOP).mk V$(TOP)
	cd obj_dir; ./V$(TOP) ../$(ELF)

clean:
	$(RM) obj_dir
