include ../../common.mk

ifeq ($(ARCH),x86_64)
	TARGET:= $(BINDIR)/BOOTX64.EFI
endif

CC:= clang
LD:= lld-link

CFLAGS:= -target $(ARCH)-pc-win32 -std=c23 -Wall -Wextra -O2
CFLAGS+= -ffreestanding -fno-stack-protector -fno-pie -fshort-wchar -fno-builtin
CFLAGS+= -MMD -MP
INCLUDE:= -Iinclude -Iinclude/$(ARCH)

LDFLAGS:= -subsystem:efi_application -entry:efi_main

SRCS:= efi_main.c ../lib/elf.c ../lib/printf.c
OBJS:= $(SRCS:%.c=%.o)

.PHONY: all

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -out:$@ $^

-include $(OBJS:.o=.d)
