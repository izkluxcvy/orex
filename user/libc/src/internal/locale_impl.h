#pragma once

#include <locale.h>

#include <pthread_impl.h>

#define LOCALE_NAME_MAX 32

struct __locale_struct {
    char name[LC_ALL][LOCALE_NAME_MAX];
    int utf8;
};

extern struct __locale_struct __global_locale;

static inline locale_t __cur_locale() {
    locale_t l = __pthread_self()->locale;
    return l ? l : &__global_locale;
}

static inline locale_t __real_locale(locale_t l) {
    return l == LC_GLOBAL_LOCALE ? &__global_locale : l;
}

#define UTF8_L(l) (__real_locale(l)->utf8)
#define UTF8      (__cur_locale()->utf8)

int __locale_kind(const char *name);
