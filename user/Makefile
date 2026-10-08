BASEDIR := ..
include ../common.mk

CC := clang
LD := ld.lld

BASE_CFLAGS := -target $(ARCH)-elf -std=c23 -masm=intel -Wall -Wextra -O2
BASE_CFLAGS += -fPIE -MMD -MP

CFLAGS := $(BASE_CFLAGS) -ffreestanding -fno-stack-protector -mgeneral-regs-only

LDFLAGS := -T user.ld -static -nostdlib

ROOT := $(BINDIR)/root
TARGET := $(BINDIR)/initramfs.cpio

LIB_OBJS := lib/crt0.o lib/ulib.o
PROGS := hello init

ULIB_BINS := $(PROGS:%=$(ROOT)/bin/%)
BINS := $(ULIB_BINS)

STATIC := $(shell find root -type f)
OBJS := $(LIB_OBJS) $(PROGS:%=bin/%.o)

.PHONY: all

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(ULIB_BINS): $(ROOT)/bin/%: bin/%.o $(LIB_OBJS) user.ld
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $< $(LIB_OBJS)

$(TARGET): owners tools/mkcpio.py $(BINS) $(STATIC)
	mkdir -p $(ROOT)
	cp -r root/. $(ROOT)
	find $(ROOT) -type d -exec chmod 0755 {} +
	find $(ROOT) -type f -exec chmod 0644 {} +
	chmod 0755 $(BINS)
	python3 tools/mkcpio.py $(ROOT) owners $(TARGET)

-include $(OBJS:.o=.d)

.SECONDARY: $(OBJS)
.DELETE_ON_ERROR:
