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

DISKROOT := $(BINDIR)/diskroot
ROOTFS := $(BINDIR)/rootfs.ext2
MKFS := /usr/sbin/mkfs.ext2
DEBUGFS := /usr/sbin/debugfs
ROOTFS_SIZE = 32M
ROOTFS_BLOCK = 1024

LIB_OBJS := lib/crt0.o lib/ulib.o
PROGS := hello init cat id

ULIB_BINS := $(PROGS:%=$(ROOT)/bin/%)
BINS := $(ULIB_BINS)

STATIC := $(shell find root -type f)
OBJS := $(LIB_OBJS) $(PROGS:%=bin/%.o)

.PHONY: all

all: $(TARGET) $(ROOTFS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(ULIB_BINS): $(ROOT)/bin/%: bin/%.o $(LIB_OBJS) user.ld
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $< $(LIB_OBJS)

$(TARGET): owners tools/mkcpio.py $(BINS) $(STATIC)
	mkdir -p $(ROOT)
	cp -r root/. $(ROOT)
	mkdir -p $(ROOT)/dev
	find $(ROOT) -type d -exec chmod 0755 {} +
	find $(ROOT) -type f -exec chmod 0644 {} +
	chmod 0755 $(BINS)
	cp $(ROOT)/bin/id $(ROOT)/bin/rootid && chmod 4755 $(ROOT)/bin/rootid
	chmod 0600 $(ROOT)/etc/shadow
	python3 tools/mkcpio.py $(ROOT) owners $(TARGET)

$(ROOTFS): $(TARGET)
	rm -rf $(DISKROOT)
	cp -a $(ROOT) $(DISKROOT)
	echo ext2 > $(DISKROOT)/etc/rootfs
	chmod 0644 $(DISKROOT)/etc/rootfs
	$(MKFS) -q -F -b $(ROOTFS_BLOCK) -d $(DISKROOT) $@ $(ROOTFS_SIZE)
	cd $(DISKROOT) && find . -mindepth 1 | sed 's|^\.||' | \
		awk '{printf "sif %s uid 0\nsif %s gid 0\n", $$0, $$0}' > ../rootfs.chown
	grep -v '^#' owners | awk 'NF == 3 {printf "sif /%s uid %s\nsif /%s gid %s\n", $$1, $$2, $$1, $$3}' >> $(BINDIR)/rootfs.chown
	$(DEBUGFS) -w -f $(BINDIR)/rootfs.chown $@ > /dev/null 2>&1

-include $(OBJS:.o=.d)

.SECONDARY: $(OBJS)
.DELETE_ON_ERROR:
