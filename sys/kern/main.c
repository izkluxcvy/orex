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

#include "../mm/pmm.h"
#include "pmap.h"

extern const uint8_t user_blob[], user_blob_end[];
__asm__(".section .rodata\n"
        "user_blob:\n"
        "    mov eax, 1\n"
        "    mov edi, 1\n"
        "    lea rsi, [rip + 1f]\n"
        "    mov edx, 18\n"
        "    syscall\n"

        "    mov byte ptr [rsp - 0x20000], 1\n"
        "    mov eax, 12\n"
        "    xor edi, edi\n"
        "    syscall\n"

        "    lea rdi, [rax + 0x3000]\n"
        "    mov eax, 12\n"
        "    syscall\n"

        "    mov byte ptr [rax - 1], 1\n"
        "    mov eax, 9\n"
        "    xor edi, edi\n"
        "    mov esi, 0x2000\n"
        "    mov edx, 3\n"
        "    mov r10d, 0x22\n"
        "    mov r8, -1\n"
        "    xor r9d, r9d\n"
        "    syscall\n"

        "    mov byte ptr [rax], 1\n"
        "    mov eax, 1\n"
        "    mov edi, 1\n"
        "    lea rsi, [rip + 2f]\n"
        "    mov edx, 13\n"
        "    syscall\n"

        // "    mov eax, 60\n"
        // "    mov edi, 3\n"
        // "    syscall\n"
        "    mov byte ptr [0x1000], 1\n"
        "1: .ascii \"hello from ring 3\\n\"\n"
        "2: .ascii \"memory works\\n\"\n"
        "user_blob_end:\n"
        ".text\n");

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

    struct proc *p =
        proc_spawn_blob(user_blob, (size_t)(user_blob_end - user_blob));
    if (p) {
        int status = proc_join(p);
        printf("main: the program exited with status 0x%x; %lu pages free\n",
               status, pmm_free_pages());
    }
}
