#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <ktime.h>
#include <machdep.h>
#include <mm.h>
#include <printf.h>
#include <proc.h>
#include <ramfs.h>
#include <sched.h>
#include <thread.h>
#include <vfs.h>

#include "pmap.h"

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);
    sched_init();
    machdep_init_late();
    time_init();

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

    static const char *const argv[] = {"/bin/hello", "one", "two", nullptr};
    static const char *const envp[] = {"PATH=/bin", nullptr};
    struct proc             *p      = proc_spawn("/bin/hello", argv, envp);
    if (!p) {
        printf("main: cannot spawn /bin/hello\n");
    } else {
        int status = proc_join(p);
        printf("main: /bin/hello exited with status %d\n", status);
    }
}
