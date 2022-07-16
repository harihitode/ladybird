TARGET=test_sim
SRCS=elfloader.c sim.c memory.c
HDRS=sim.h memory.h elfloader.h
OBJS=$(SRCS:.c=.o)
CFLAGS=-O3 -Wall
LDLIBS=-lm

.PHONY: all clean
.INTERMEDIATE: $(OBJS) $(TARGET).o

all: $(TARGET)

$(TARGET): $(OBJS) $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(TARGET).o -o $(TARGET) $(LDLIBS)

$(TARGET).o: sim.h

$(OBJS): $(HDRS)

clean:
	$(RM) $(TARGET)
