BASEDIR := .
include common.mk

SUBDIRS := stand sys

.PHONY: all pre clean run_qemu $(SUBDIRS)

all: pre $(SUBDIRS)
	@echo "Build complete"

pre:
	mkdir -p $(BINDIR)

$(SUBDIRS):
	cd $@ && $(MAKE) || exit 1

run_qemu: all
	DISK_IMAGE=$(BASEDIR)/disk.img; \
	MOUNT_POINT=$(BASEDIR)/mnt; \
	OVMF_DIR=$(BASEDIR)/ovmf; \
	LOADER_BIN=$(BINDIR)/loader.efi; \
	if [ $(ARCH) = "x86_64" ]; then \
		EFI_BIN_NAME=BOOTX64.EFI; \
	fi; \
	KERNEL_BIN=$(BINDIR)/kernel.elf; \
	\
	rm -rf $$DISK_IMAGE; \
	qemu-img create -f raw $$DISK_IMAGE 200M; \
	sudo mkfs.fat -n 'OREX' -s 2 -f 2 -R 32 -F 32 $$DISK_IMAGE; \
	mkdir -p $$MOUNT_POINT; \
	sudo mount -o loop $$DISK_IMAGE $$MOUNT_POINT; \
	sudo mkdir -p $$MOUNT_POINT/EFI/BOOT; \
	sudo cp $$LOADER_BIN $$MOUNT_POINT/EFI/BOOT/$$EFI_BIN_NAME; \
	sudo cp $$KERNEL_BIN $$MOUNT_POINT/; \
	sudo umount $$MOUNT_POINT; \
	\
	qemu-system-$(ARCH) \
	    -m 1G \
	    -drive if=pflash,format=raw,readonly=on,file=$$OVMF_DIR/OVMF_CODE.fd \
	    -drive if=pflash,format=raw,file=$$OVMF_DIR/OVMF_VARS.fd \
	    -drive if=ide,index=0,media=disk,format=raw,file=$$DISK_IMAGE \
	    -monitor stdio

clean:
	find . -name '*.o' -delete
	find . -name '*.d' -delete
