BASEDIR := ../..
include ../../common.mk

TARGET := $(BINDIR)/loader.efi

CC := clang
LD := lld-link

CFLAGS := -target $(ARCH)-pc-win32
CFLAGS += -std=c23 -Wall -Wextra -O2
CFLAGS += -ffreestanding -fno-stack-protector -fno-pie -fshort-wchar -fno-builtin
CFLAGS += -MMD -MP
INCLUDE := -Iinclude -Iinclude/$(ARCH) -I../include

LDFLAGS := -subsystem:efi_application -entry:efi_main

SRCS := efi_main.c ../lib/elf.c ../lib/printf.c ../arch/$(ARCH)/machdep.c ../arch/$(ARCH)/paging.c
OBJS := $(SRCS:%.c=%.o)

.PHONY: all

all: $(TARGET)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -out:$@ $(OBJS)

-include $(OBJS:.o=.d)
