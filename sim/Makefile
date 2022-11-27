SRCS=elfloader.c sim.c memory.c csr.c mmio.c plic.c core.c trigger.c riscv.c
HDRS=sim.h memory.h elfloader.h csr.h mmio.h plic.h core.h trigger.h riscv.h
OBJS=$(SRCS:.c=.o)
POBJS=launch_sim.o gdbstub_sys.o
STUBSRCS=gdbstub/gdbstub.c
STUBHDRS=gdbstub/gdbstub.h
STUBOBJS=$(STUBSRCS:.c=.o)
CFLAGS=-O3 -Wall -I./
LDLIBS=-lm -lpthread
RVPATH?=~/llvm-riscv/bin
XV6KRNL=../../ladybird_xv6/kernel/kernel
XV6DISK=../../ladybird_xv6/fs.img
KRNL?=$(XV6KRNL)
DISK?=$(XV6DISK)
PORT=12345
COMMANDS?=

.PHONY: all clean xv6 run_rspsim run_lldb
.INTERMEDIATE: $(OBJS) $(POBJS)
.SILENT: run_rspsim run_lldb

all: launch_sim rspsim

launch_sim: $(OBJS) launch_sim.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

rspsim: $(OBJS) $(STUBOBJS) gdbstub_sys.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

xv6: launch_sim
	./$^ $(XV6KRNL) $(XV6DISK) $(COMMANDS)

$(POJBS): sim.h

$(OBJS): $(HDRS)

run_rspsim: rspsim
	./rspsim $(KRNL) $(DISK) $(COMMANDS)

run_lldb:
	$(RVPATH)/lldb $(KRNL) -o 'process connect connect://localhost:$(PORT)'

clean:
	$(RM) launch_sim rspsim *.o
