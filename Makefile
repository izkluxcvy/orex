include common.mk

SUBDIRS:= stand kernel

.PHONY: all clean run_qemu $(SUBDIRS)

all: $(SUBDIRS)
	@echo "Build complete"

$(SUBDIRS):
	cd $@ && $(MAKE) || exit 1

run_qemu: all
	DISK_IMAGE=$(ROOT)/disk.img; \
	MOUNT_POINT=$(ROOT)/mnt; \
	OVMF_DIR=$(ROOT)/ovmf; \
	if [ $(ARCH) = "x86_64" ]; then \
		EFI_BIN=$(BINDIR)/BOOTX64.EFI; \
	fi; \
	KERNEL_BIN=$(BINDIR)/kernel.elf; \
	\
	rm -rf $$DISK_IMAGE; \
	qemu-img create -f raw $$DISK_IMAGE 200M; \
	sudo mkfs.fat -n 'OREX' -s 2 -f 2 -R 32 -F 32 $$DISK_IMAGE; \
	mkdir -p $$MOUNT_POINT; \
	sudo mount -o loop $$DISK_IMAGE $$MOUNT_POINT; \
	sudo mkdir -p $$MOUNT_POINT/EFI/BOOT; \
	sudo cp $$EFI_BIN $$MOUNT_POINT/EFI/BOOT/; \
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
