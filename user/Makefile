BASEDIR := ..
include ../common.mk

CC := clang
LD := ld.lld
AR := llvm-ar

BASE_CFLAGS := -target $(ARCH)-elf -std=c23 -masm=intel -Wall -Wextra -O2
BASE_CFLAGS += -fPIE -MMD -MP

CFLAGS := $(BASE_CFLAGS) -ffreestanding -fno-stack-protector -mgeneral-regs-only

LIBC_INC := -nostdlibinc -isystem libc/include
SSP := -fstack-protector-strong
LIBC_CFLAGS := $(BASE_CFLAGS) $(LIBC_INC) -Ilibc/src/internal -ffreestanding
LIBC_CFLAGS += $(SSP)
APP_CFLAGS := $(BASE_CFLAGS) $(LIBC_INC) $(SSP)

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
PROGS := hello init cat id ls fs share

LIBC := libc/libc.a
CRT1 := libc/crt/crt1.o
LIBC_SRCS := $(shell find libc/src -name '*.c')
LIBC_OBJS := $(patsubst %.c,%.o,$(filter-out libc/src/ldso/dynlink.c,$(LIBC_SRCS)))

ULIB_BINS := $(PROGS:%=$(ROOT)/bin/%)
BINS := $(ULIB_BINS)

STATIC := $(shell find root -type f)
OBJS := $(LIB_OBJS) $(PROGS:%=bin/%.o)
OBJS += $(CRT1) $(LIBC_OBJS)

.PHONY: all

all: $(TARGET) $(ROOTFS) $(LIBC) $(CRT1)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(ULIB_BINS): $(ROOT)/bin/%: bin/%.o $(LIB_OBJS) user.ld
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $< $(LIB_OBJS)

$(LIBC_OBJS) $(CRT1): CFLAGS := $(LIBC_CFLAGS)

$(LIBC): $(LIBC_OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(TARGET): owners tools/mkcpio.py $(BINS) $(STATIC)
	mkdir -p $(ROOT)
	cp -r root/. $(ROOT)
	mkdir -p $(ROOT)/dev $(ROOT)/mnt $(ROOT)/run $(ROOT)/tmp
	find $(ROOT) -type d -exec chmod 0755 {} +
	find $(ROOT) -type f -exec chmod 0644 {} +
	chmod 0755 $(BINS)
	cp $(ROOT)/bin/id $(ROOT)/bin/rootid && chmod 4755 $(ROOT)/bin/rootid
	chmod 0600 $(ROOT)/etc/shadow
	chmod 1777 $(ROOT)/tmp
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
