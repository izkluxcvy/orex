#include <stdint.h>

#include <blk.h>
#include <boot_info.h>
#include <cmdline.h>
#include <console.h>
#include <devfs.h>
#include <errno.h>
#include <ext2.h>
#include <ktime.h>
#include <machdep.h>
#include <memcmp.h>
#include <mm.h>
#include <panic.h>
#include <pci.h>
#include <printf.h>
#include <proc.h>
#include <ramfs.h>
#include <sched.h>
#include <thread.h>
#include <tmpfs.h>
#include <tty.h>
#include <vfs.h>
#include <virtio.h>

#include "pmap.h"

static int strcmp_k(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++, b++;
    }
    return *a != *b;
}

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    cmdline_init(bi.cmdline);
    mm_init(&bi);
    console_init(&bi.fb);
    sched_init();
    machdep_init_late();
    time_init();
    pci_init();
    virtio_blk_init();

    blk_scan_partitions();
    char console[16];
    if (cmdline_get("console", console, sizeof(console))) {
        tty_console_select(console);
    }
    char want[64] = "";
    int named = cmdline_get("root", want, sizeof(want));
    int ro = cmdline_get("ro", nullptr, 0);
    struct fs *root = nullptr;
    int from_initramfs = 0;
    int ram = named && !strcmp_k(want, "ram");
    for (int i = 0; !ram && !root && blk_at(i); i++) {
        struct blkdev *d = blk_at(i);
        char uuid[37];
        if (ext2_uuid(d, uuid) != 0) {
            continue;
        }
        const char *w = want[0] == '/' ? want + 5 : want;
        if (named && !(memcmp(want, "UUID=", 5) == 0 ? !strcmp_k(want + 5, uuid)
                                                     : !strcmp_k(w, d->name))) {
            continue;
        }
        int clean = ext2_clean(d);
        if ((root = ext2_mount(d, ro || !clean)) != nullptr) {
            printf("main: root is ext2 on %s%s\n", d->name,
                   ro || !clean ? " (read-only)" : "");
        }
    }
    if (named & 7 && !ram && !root) {
        printf("main: no root %s\n", want);
    }

    if (!root) {
        if (bi.initrd_size) {
            root = ramfs_create(phys_to_virt(bi.initrd_base), bi.initrd_size);
            printf("main: root is ramfs on initrd, %lu bytes\n",
                   (unsigned long)bi.initrd_size);
        } else {
            printf("main: no disk to be the root\n");
        }
        from_initramfs = 1;
    }
    if (!root || vfs_mount_root(root) != 0) {
        printf("main: cannot mount root filesystem\n");
    }
    int err = vfs_mount("/dev", devfs_create());
    if (err) {
        printf("main: cannot mount devfs on /dev: %d\n", err);
    }

    if ((err = vfs_mount("/run", tmpfs_create(0755))) != 0 && err != -ENOENT) {
        printf("main: cannot mount tmpfs on /run: %d\n", err);
    }
    if (from_initramfs && (err = vfs_mount("/tmp", tmpfs_create(01777))) != 0) {
        printf("main: cannot mount tmpfs on /tmp: %d\n", err);
    }

    blk_start_flusher();

    static const char *const argv[] = {"/bin/init", nullptr};
    static const char *const envp[] = {"PATH=/bin", nullptr};
    if (!proc_spawn("/bin/init", argv, envp)) {
        printf("main: cannot spawn init\n");
    }

    while (1) {
        int status;
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
