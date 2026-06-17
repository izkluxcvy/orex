include ../../common.mk

ifeq ($(ARCH),amd64)
	CARCH := x86_64
	TARGET := $(BINDIR)/BOOTX64.EFI
endif

CC := clang
CFLAGS := -target $(CARCH)-pc-win32 -std=c11  -Wall -Wextra -O2
CFLAGS += -ffreestanding -fno-stack-protector -fno-pie -fshort-wchar -fno-builtin
CFLAGS += -MMD -MP

CFLAGS += -std=c11
INCLUDE := -Iinclude -Iinclude/$(ARCH)

SRCS := efi_main.c
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)

.PHONY: all

all: $(TARGET)

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

$(TARGET): $(OBJS)
	# $(CC) $(CFLAGS) -o $@ $^
	lld-link -subsystem:efi_application -entry:efi_main -out:$@ $^

-include $(OBJS:.o=.d)
