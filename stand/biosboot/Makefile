BASEDIR := ../..
include ../../common.mk

MBR := $(BINDIR)/bootmbr.bin
STAGE2 := $(BINDIR)/bootbios.bin

CC := clang
LD := ld.lld

CFLAGS := -target $(ARCH)-elf -std=c23 -Wall -Wextra -Os -masm=intel
CFLAGS += -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mgeneral-regs-only
CFLAGS += -mcmodel=small -fno-builtin
CFLAGS += -MMD -MP
INCLUDE := -I../include

OBJS := entry.o main.o ../lib/elf.o64 ../lib/printf.o64 ../arch/$(ARCH)/paging.o64

.PHONY: all

all: $(MBR) $(STAGE2)

%.o: %.S
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

%.o64: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

$(MBR): mbr.o mbr.ld
	$(LD) -T mbr.ld -o $@ mbr.o
	test $$(stat -c %s $@) -le 440

$(STAGE2): $(OBJS) stage2.ld
	$(LD) -T stage2.ld -o $@ $(OBJS)

-include $(OBJS:=.d)
