#include <stddef.h>
#include <stdint.h>

#include <errno.h>
#include <uaccess.h>

#include "../mm/pmm.h"
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

long copy_str_from_user(char *dst, const char *usrc, size_t max) {
    for (size_t i = 0; i < max;) {
        uintptr_t p     = (uintptr_t)(usrc + i);
        size_t    chunk = PAGE_SIZE - (p & (PAGE_SIZE - 1));
        if (chunk > max - i) {
            chunk = max - i;
        }
        if (!user_range(usrc + i, chunk) ||
            __uaccess_copy(dst + i, usrc + i, chunk) != 0) {
            return -EFAULT;
        }
        for (size_t k = 0; k < chunk; k++, i++) {
            if (!dst[i]) {
                return (long)i;
            }
        }
    }
    return -ENAMETOOLONG;
}
