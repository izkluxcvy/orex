#pragma once

static inline char *__fmt_ulong(unsigned long v, char *end) {
    do {
        *--end = (char)('0' + (v % 10));
    } while (v /= 10);
    return end;
}
