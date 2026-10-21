#include <stdint.h>

#include <blk.h>
#include <boot_info.h>
#include <console.h>
#include <devfs.h>
#include <kmalloc.h>
#include <ktime.h>
#include <machdep.h>
#include <mm.h>
#include <panic.h>
#include <pci.h>
#include <printf.h>
#include <proc.h>
#include <ramfs.h>
#include <sched.h>
#include <thread.h>
#include <vfs.h>
#include <virtio.h>

#include "pmap.h"

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);
    sched_init();
    machdep_init_late();
    time_init();
    pci_init();
    virtio_blk_init();

    uint8_t *sector = kmalloc(512);
    for (int i = 0; sector && blk_at(i); i++) {
        struct blkdev *d = blk_at(i);
        if (d->read(d, 2, 1, sector) == 0) {
            printf("main: %s: bytes 1080-1081 are %02x %02x\n", d->name,
                   sector[56], sector[57]);
        }
    }
    kfree(sector);

    struct fs *root = nullptr;
    if (bi.initrd_size) {
        root = ramfs_create(phys_to_virt(bi.initrd_base), bi.initrd_size);
        printf("main: root is the initrd, %lu bytes\n",
               (unsigned long)bi.initrd_size);
    } else {
        printf("main: no disk to be the root\n");
    }
    if (!root || vfs_mount_root(root) != 0) {
        printf("main: cannot mount root filesystem\n");
    }
    int err = vfs_mount("/dev", devfs_create());
    if (err) {
        printf("main: cannot mount devfs on /dev: %d\n", err);
    }

    static const char *const argv[] = {"/bin/init", nullptr};
    static const char *const envp[] = {"PATH=/bin", nullptr};
    if (!proc_spawn("/bin/init", argv, envp)) {
        printf("main: cannot spawn init\n");
    }

    while (1) {
        int   status;
        pid_t pid = proc_wait(-1, &status, WEXITED, nullptr, nullptr);
        if (pid < 0) {
            break;
        }
        if (status & 0x7f) {
            printf("main: pid %d terminated by signal %d\n", pid,
                   status & 0x7f);
        } else {
            printf("main: pid %d exited with status %d\n", pid,
                   (status >> 8) & 0xff);
        }
        if (pid == 1) {
            panic("main: init exited");
        }
    }
    panic("main: no init");
}
