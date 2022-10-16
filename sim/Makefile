TARGET=launch_sim
SRCS=elfloader.c sim.c memory.c csr.c mmio.c plic.c
HDRS=sim.h memory.h elfloader.h csr.h mmio.h plic.h
OBJS=$(SRCS:.c=.o)
STUBSRCS=gdbstub/gdbstub.c
STUBHDRS=gdbstub/gdbstub.h
STUBOBJS=$(STUBSRCS:.c=.o)
CFLAGS=-O3 -Wall -I./
LDLIBS=-lm -lpthread
RVPATH?=~/llvm-riscv/bin
PROGRAM?=../../ladybird_xv6/kernel/kernel
DISK?=../../ladybird_xv6/fs.img
PORT=12345

.PHONY: all clean xv6 run_rspsim run_lldb
.INTERMEDIATE: $(OBJS) $(TARGET).o
.SILENT: run_rspsim run_lldb

all: $(TARGET)

$(TARGET): $(OBJS) $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TARGET).o -o $(TARGET) $(LDLIBS)

rspsim: $(OBJS) $(STUBOBJS) gdbstub_sys.o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

xv6: $(TARGET)
	./$(TARGET) ../img/kernel ../img/fs.img

$(TARGET).o: sim.h

$(OBJS): $(HDRS)

run_rspsim: rspsim
	./rspsim $(PROGRAM) $(DISK)

run_lldb:
	$(RVPATH)/lldb $(PROGRAM) -o 'process connect connect://localhost:$(PORT)'

clean:
	$(RM) $(TARGET) rspsim
