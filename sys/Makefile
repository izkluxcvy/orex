BASEDIR := ..
include ../common.mk

TARGET := $(BINDIR)/kernel.elf

CC := clang
LD := ld.lld

CFLAGS := -target $(ARCH)-elf
CFLAGS += -std=c23 -Wall -Wextra -O2
CFLAGS += -ffreestanding -fno-stack-protector -fno-pie -mno-red-zone
CFLAGS += -MMD -MP

LDFLAGS := -T arch/$(ARCH)/kernel.ld -z norelro --image-base=0x100000 --static

SRCS := kernel/kern_main.c
OBJS := $(SRCS:%.c=%.o)

.PHONY: all

all: $(TARGET)

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
