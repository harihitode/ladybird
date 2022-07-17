TOOLPREFIX = ${HOME}/llvm-riscv/bin/
CC = ${TOOLPREFIX}clang
AS = ${TOOLPREFIX}clang -c
AR = ${TOOLPREFIX}llvm-ar
LD = ${TOOLPREFIX}ld.lld
OBJCOPY = ${TOOLPREFIX}llvm-objcopy
OBJDUMP = ${TOOLPREFIX}llvm-objdump

ASFLAGS = -nodefaultlibs -O3 -g -static -I. -march=rv32imaf -mabi=ilp32f
CFLAGS += $(ASFLAGS) -fno-builtin
LDFLAGS = -nodefaultlibs -T ladybird.ld -L./ -z nognustack
ARFLAGS = rcs

TARGET ?= hello
TARGET_OBJDUMP = ${TARGET:=.objdump}

.PHONY: all clean

all: $(TARGET) $(TARGET_OBJDUMP)

$(TARGET): crt0.o $(TARGET).o

$(TARGET_OBJDUMP): %.objdump: %
	$(OBJDUMP) -S $< > $@

clean:
	rm -rf *.o *.objdump $(TARGET)
