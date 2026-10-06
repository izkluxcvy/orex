#include <stdarg.h>

#include <cpu.h>
#include <printf.h>

#include "irq.h"

static void (*sinks[PRINTF_MAX_SINKS])(char c);
static int sink_count;

static volatile int out_cpu = -1;

static int out_lock(uint64_t *flags) {
    *flags = irq_save();
    int me = curcpu()->id;
    if (out_cpu == me) {
        return 0;
    }
    int free = -1;
    while (!__atomic_compare_exchange_n(&out_cpu, &free, me, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        free = -1;
        __asm__ __volatile__("pause");
    }
    return 1;
}

static void out_unlock(int taken, uint64_t flags) {
    if (taken) {
        __atomic_store_n(&out_cpu, -1, __ATOMIC_RELEASE);
    }
    irq_restore(flags);
}

void printf_add_sink(void (*putc)(char c)) {
    if (sink_count < PRINTF_MAX_SINKS) {
        sinks[sink_count++] = putc;
    }
}

static void emit(char c) {
    for (int i = 0; i < sink_count; i++) {
        if (c == '\n') {
            sinks[i]('\r');
        }
        sinks[i](c);
    }
}

void printf_write(const char *s, size_t n) {
    uint64_t flags;
    int      taken = out_lock(&flags);
    for (size_t i = 0; i < n; i++) {
        emit(s[i]);
    }
    out_unlock(taken, flags);
}

const char *digits = "0123456789abcdef";
static void print_int(unsigned long val, int base, int width, char pad) {
    char  buf[32];
    char *p = buf;

    do {
        *p++ = digits[val % base];
    } while (val /= base);
    for (int i = (int)(p - buf); i < width; i++) {
        emit(pad);
    }

    while (p > buf) {
        emit(*--p);
    }
}

void vprintf(const char *fmt, va_list args) {
    uint64_t flags;
    int      taken = out_lock(&flags);

    for (char *p = (char *)fmt; *p; p++) {
        if (*p != '%') {
            emit(*p);
            continue;
        }

        p++;

        char pad   = ' ';
        int  width = 0;
        if (*p == '0') {
            pad = '0';
            p++;
        }
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p++ - '0');
        }

        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
        }

        switch (*p) {
        case 'd': {
            long val = is_long ? va_arg(args, long) : va_arg(args, int);
            if (val < 0) {
                emit('-');
                val = -val;
                width--;
            }
            print_int(val, 10, width, pad);
            break;
        }
        case 'u': {
            unsigned long val = is_long ? va_arg(args, unsigned long)
                                        : va_arg(args, unsigned int);
            print_int(val, 10, width, pad);
            break;
        }
        case 'x': {
            unsigned long val = is_long ? va_arg(args, unsigned long)
                                        : va_arg(args, unsigned int);
            print_int(val, 16, width, pad);
            break;
        }
        case 'p': {
            unsigned long val = (unsigned long)va_arg(args, void *);
            print_int(val, 16, width, '0');
            break;
        }
        case 's': {
            char *s = va_arg(args, char *);
            while (*s) {
                emit(*s++);
            }
            break;
        }
        }
    }
    out_unlock(taken, flags);
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
