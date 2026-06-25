include ../common.mk

TARGET:= $(BINDIR)/kernel.elf

CC:= clang
LD:= ld.lld

CFLAGS:= -target $(ARCH)-elf -std=c23 -Wall -Wextra -O2
CFLAGS+= -ffreestanding -fno-stack-protector -fno-pie -mno-red-zone
CFLAGS+= -MMD -MP

LDFLAGS:= --entry kern_main -z norelro --image-base=0x100000 --static

SRCS:= main.c
OBJS:= $(SRCS:%.c=%.o)

.PHONY: all

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
