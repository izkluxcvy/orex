#include <stdint.h>

#include <boot_info.h>
#include <console.h>
#include <ktime.h>
#include <machdep.h>
#include <memcpy.h>
#include <mm.h>
#include <printf.h>
#include <sched.h>
#include <thread.h>

#include "../mm/pmm.h"
#include "irq.h"
#include "pmap.h"
#include "user.h"

extern const uint8_t user_blob[], user_blob_end[];
__asm__(".section .rodata\n"
        "user_blob:\n"
        // "    mov rax, [0xffffffff81000000]\n"
        "    mov eax, 1\n"
        "    mov edi, 1\n"
        "    lea rsi, [rip + 1f]\n"
        // "    mov rsi, 0xffffffff81000000\n"
        "    mov edx, 18\n"
        "    syscall\n"
        "    mov eax, 60\n"
        "    xor edi, edi\n"
        "    syscall\n"
        "1: .ascii \"hello from ring 3\\n\"\n"
        "user_blob_end:\n"
        ".text\n");

#define USER_CODE 0x400000UL

static void *user_thread(void *arg) {
    struct pmap *pm    = arg;
    uintptr_t    code  = pmm_alloc_page();
    uintptr_t    stack = pmm_alloc_page();
    memcpy(phys_to_virt(code), user_blob, (size_t)(user_blob_end - user_blob));
    pmap_enter(pm, USER_CODE, code, pmap_user_flags(0, 1));
    pmap_enter(pm, USER_STACK_TOP - PAGE_SIZE, stack, pmap_user_flags(1, 0));

    uint64_t flags  = irq_save();
    curthread->pmap = pm;
    pmap_activate(pm);
    irq_restore(flags);
    usermode_enter(USER_CODE, USER_STACK_TOP);
}

void kern_main(struct boot_info *boot_info) {
    struct boot_info bi = *boot_info;

    machdep_init();
    mm_init(&bi);
    console_init(&bi.fb);
    sched_init();
    machdep_init_late();
    time_init();

    struct pmap   *pm = pmap_create();
    struct thread *t  = thread_create("user", user_thread, pm, SCHED_OTHER, 0);
    thread_join(t, nullptr);
    pmap_destroy(pm);
    printf(
        "orex: the program is gone, and its memory with it: %lu pages free\n",
        pmm_free_pages());
}
