SRCS=elfloader.c sim.c memory.c csr.c mmio.c plic.c core.c trigger.c riscv.c htif.c
HDRS=sim.h memory.h elfloader.h csr.h mmio.h plic.h core.h trigger.h riscv.h htif.h
OBJS=$(SRCS:.c=.o)
POBJS=launch_sim.o gdbstub_sys.o
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

ifeq ($(PROFILE),1)
	CFLAGS+=-pg
endif

.PHONY: all clean xv6 run_rspsim run_lldb
.INTERMEDIATE: $(OBJS) $(POBJS)
.SILENT: run_rspsim run_lldb

all: launch_sim rspsim

launch_sim: $(OBJS) launch_sim.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

rspsim: $(OBJS) $(STUBOBJS) gdbstub_sys.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

xv6: launch_sim
	./$< $(XV6KRNL) --ebreak --disk $(XV6DISK) --uart-in $(COMMANDS)

linux: launch_sim ladybird.dtb
	./$< $(LINUXKRNL) --disk $(LINUXDISK) --htif --tohost 0x80040f88 --fromhost 0x80040f90

$(POJBS): sim.h

$(OBJS): $(HDRS)

run_rspsim: rspsim
	./rspsim $(KRNL) --ebreak --disk $(DISK) --uart-in $(COMMANDS)

run_lldb:
	$(RVPATH)/lldb $(KRNL) -o 'process connect connect://localhost:$(PORT)'

ladybird.dtb: ladybird.dts
	dtc -O dtb -o ladybird.dtb ladybird.dts

clean:
	$(RM) launch_sim rspsim *.o
