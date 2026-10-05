#pragma once

#include <stddef.h>

size_t            __uaccess_copy(void *dst, const void *src, size_t n);
extern const char __uaccess_begin[], __uaccess_end[], __uaccess_fixup[];

int copy_from_user(void *dst, const void *usrc, size_t n);
int copy_to_user(void *udst, const void *src, size_t n);
