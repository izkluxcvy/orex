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

SRCS := kern/main.c kern/cpu.c kern/panic.c kern/spinlock.c kern/thread.c \
		kern/sched.c kern/waitq.c kern/mutex.c kern/time.c kern/syscall.c \
		kern/vfs.c kern/vm.c kern/proc.c kern/exec.c kern/fd.c
SRCS += arch/$(ARCH)/entry.c arch/$(ARCH)/init.c arch/$(ARCH)/serial.c \
		arch/$(ARCH)/mem_.c arch/$(ARCH)/gdt.c arch/$(ARCH)/idt.c \
		arch/$(ARCH)/trap.c arch/$(ARCH)/pmap.c arch/$(ARCH)/irq.c \
		arch/$(ARCH)/apic.c arch/$(ARCH)/switch.c arch/$(ARCH)/rtc.c \
		arch/$(ARCH)/uaccess.c arch/$(ARCH)/user.c
SRCS += mm/init.c mm/pmm.c mm/kmalloc.c
SRCS += dev/console.c
SRCS += fs/ramfs.c fs/devfs.c
SRCS += lib/printf.c lib/memcmp.c
OBJS := $(SRCS:.c=.o)

.PHONY: all

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

$(TARGET): $(OBJS) arch/$(ARCH)/kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

-include $(OBJS:.o=.d)
