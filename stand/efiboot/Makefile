include ../../common.mk

CARCH:= $(ARCH)
ifeq ($(ARCH),amd64)
	CARCH:= x86_64
	TARGET:= $(BINDIR)/BOOTX64.EFI
endif

CC:= clang
LD:= lld-link
CFLAGS:= -target $(CARCH)-pc-win32 -std=c11  -Wall -Wextra -O2
CFLAGS+= -ffreestanding -fno-stack-protector -fno-pie -fshort-wchar -fno-builtin
CFLAGS+= -MMD -MP

INCLUDE:= -Iinclude -Iinclude/$(ARCH)

SRCS:= efi_main.c
OBJS:= $(SRCS:%.c=$(OBJDIR)/%.o)

.PHONY: all

all: $(TARGET)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

$(TARGET): $(OBJS)
	$(LD) -subsystem:efi_application -entry:efi_main -out:$@ $^

-include $(OBJS:.o=.d)
