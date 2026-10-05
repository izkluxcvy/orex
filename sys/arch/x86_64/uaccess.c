#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <uaccess.h>

#include "pmap.h"

__asm__(".text\n"
        ".global __uaccess_copy\n"
        ".type __uaccess_copy, @function\n"
        "__uaccess_copy:\n"
        "    mov rcx, rdx\n"
        ".global __uaccess_begin\n"
        "__uaccess_begin:\n"
        "    rep movsb\n"
        ".global __uaccess_end\n"
        "__uaccess_end:\n"
        "    xor eax, eax\n"
        "    ret\n"
        ".global __uaccess_fixup\n"
        "__uaccess_fixup:\n"
        "    mov rax, rcx\n"
        "    ret\n");

static int user_range(const void *uaddr, size_t n) {
    uintptr_t va = (uintptr_t)uaddr;
    return va >= USER_BASE && va < USER_TOP && n <= USER_TOP - va;
}

int copy_from_user(void *dst, const void *usrc, size_t n) {
    if (!user_range(usrc, n)) {
        return -EFAULT;
    }
    return __uaccess_copy(dst, usrc, n) ? -EFAULT : 0;
}

int copy_to_user(void *udst, const void *src, size_t n) {
    if (!user_range(udst, n)) {
        return -EFAULT;
    }
    return __uaccess_copy(udst, src, n) ? -EFAULT : 0;
}
