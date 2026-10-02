BASEDIR := ..
include ../common.mk

TARGET := $(BINDIR)/kernel.elf

CC := clang
LD := ld.lld

CFLAGS := -target $(ARCH)-elf -std=c23 -Wall -Wextra -O2 -masm=intel
CFLAGS += -ffreestanding -fno-stack-protector -fno-pie -mno-red-zone \
		  -mgeneral-regs-only -mcmodel=kernel
CFLAGS += -MMD -MP

INCLUDE := -I../stand/include -Iinclude -Iarch/$(ARCH)

LDFLAGS := -T arch/$(ARCH)/kernel.ld -z norelro --image-base=0x100000 --static

SRCS := kern/main.c
SRCS += arch/$(ARCH)/entry.c arch/$(ARCH)/init.c arch/$(ARCH)/serial.c \
		arch/$(ARCH)/mem_.c
SRCS += lib/printf.c
OBJS := $(SRCS:.c=.o)

.PHONY: all

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

$(TARGET): $(OBJS) arch/$(ARCH)/kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

-include $(OBJS:.o=.d)
