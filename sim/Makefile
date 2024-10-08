SRCS=elfloader.c sim.c memory.c csr.c mmio.c plic.c core.c lsu.c trigger.c riscv.c htif.c
HDRS=sim.h memory.h elfloader.h csr.h mmio.h plic.h core.h lsu.h trigger.h riscv.h htif.h
TARGET?=
OBJS=$(SRCS:.c=.o)
STUBSRCS=gdbstub/gdbstub.c
STUBHDRS=gdbstub/gdbstub.h
STUBOBJS=$(STUBSRCS:.c=.o)
CFLAGS=-O3 -Wall -I./
LDLIBS=-lm -lpthread
RVPATH?=~/llvm-riscv/bin
XV6KRNL?=./kernel
XV6DISK?=./fs.img
LINUXKRNL?=./fw_payload.elf
LINUXDISK?=./rootfs.img
KRNL?=$(XV6KRNL)
DISK?=$(XV6DISK)
PORT?=12345
PROFILE=0
COMMANDS?=
DTBS=ladybird.dtb ladybird_dual.dtb
DUALCORE?=0
TRICORE?=0
SIMFLAGS?=

ifeq ($(PROFILE),1)
	CFLAGS+=-pg
endif

ifeq ($(DUALCORE),1)
	SIMFLAGS+=--cores 2
endif

.PHONY: all clean xv6 run_rspsim run_lldb
.INTERMEDIATE: $(OBJS) $(POBJS)
.SILENT: run_rspsim run_lldb
.SUFFIXES: .dts .dtb

all: launch_sim rspsim $(DTBS)

launch_sim: $(OBJS) launch_sim.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

rspsim: $(OBJS) $(STUBOBJS) gdbstub_sys.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

rvtest: launch_sim
	./$< $(TARGET) --rvtest --tohost 0x80001000 --fromhost 0x80001040

xv6: launch_sim $(DTBS)
	./$< $(XV6KRNL) $(SIMFLAGS) --ebreak --timer --disk $(XV6DISK) --uart-in $(COMMANDS)

linux: launch_sim $(DTBS)
	./$< $(LINUXKRNL) $(SIMFLAGS) --disk $(LINUXDISK) --htif --tohost 0x80040f88 --fromhost 0x80040f90 --timer

$(POJBS): sim.h

$(OBJS): $(HDRS)

run_rspsim: rspsim
	./rspsim $(KRNL) --ebreak --disk $(DISK) --uart-in $(COMMANDS)

run_lldb:
	$(RVPATH)/lldb $(KRNL) -o 'process connect connect://localhost:$(PORT)'

.dts.dtb:
	dtc -O dtb -o $@ $<

clean:
	$(RM) launch_sim rspsim *.o
