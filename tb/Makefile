PACKAGES=ladybird_config.svh ladybird_riscv_helper.svh
INTERFACE=ladybird_axi_interface
TOP=ladybird_tb
OBJDIR=build
ELF=./hello.riscv
VDEF=-DLADYBIRD_SIMULATION -DLADYBIRD_SIMULATION_DEBUG_UNTOUCHED_MEM -DLADYBIRD_SIMULATION_HTIF
# VDEF+=-DLADYBIRD_SIMULATION_DEBUG_DUMP
TIMEOUT?=10000000
WAVE?=0

ifeq ($(WAVE), 1)
VCDFILE="./wave.vcd"
else
VCDFILE=
endif

.PHONY: all wave clean

all:
	verilator -Wall $(VDEF) --timing --cc --exe $(TOP).cpp -I../src $(PACKAGES) $(INTERFACE) --trace --top-module $(TOP) $(TOP).sv
	cd obj_dir; make -j --silent -f V$(TOP).mk V$(TOP)
	cd obj_dir; ./V$(TOP) ../$(ELF) $(TIMEOUT) $(VCDFILE)

wave:
	gtkwave ./obj_dir/wave.vcd

clean:
	$(RM) -rf obj_dir
