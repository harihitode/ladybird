TARGET=launch_sim
SRCS=elfloader.c sim.c memory.c csr.c mmio.c plic.c
HDRS=sim.h memory.h elfloader.h csr.h mmio.h plic.h
OBJS=$(SRCS:.c=.o)
CFLAGS=-O3 -Wall
LDLIBS=-lm -lpthread

.PHONY: all clean xv6
.INTERMEDIATE: $(OBJS) $(TARGET).o

all: $(TARGET)

$(TARGET): $(OBJS) $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TARGET).o -o $(TARGET) $(LDLIBS)

xv6: $(TARGET)
	./$(TARGET) ../img/kernel ../img/fs.img

$(TARGET).o: sim.h

$(OBJS): $(HDRS)

clean:
	$(RM) $(TARGET)
