BASEDIR := .
include common.mk

SUBDIRS := stand sys

DISK := $(BINDIR)/disk.img
MKDISK := python3 stand/tools/mkdisk.py

.PHONY: all disk run clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	mkdir -p $(BINDIR)
	$(MAKE) -C $@

disk: all
	test -f $(BINDIR)/cmdline.txt || echo "console=ttyS0" > $(BINDIR)/cmdline.txt
	$(MKDISK) $(DISK) --mbr $(BINDIR)/bootmbr.bin --stage2 $(BINDIR)/bootbios.bin \
		KERNEL.ELF=$(BINDIR)/kernel.elf CMDLINE.TXT=$(BINDIR)/cmdline.txt

run: disk
	qemu-system-$(ARCH) -m 1G -drive format=raw,file=$(DISK) -serial stdio

clean:
	find . -name '*.o' -delete
	find . -name '*.o64' -delete
	find . -name '*.d' -delete
	rm -rf $(BINDIR)
