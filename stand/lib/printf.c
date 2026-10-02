#include <stdarg.h>
#include <stdint.h>

#include "printf.h"

void (*printf_putc)(char c);

static const char *digits = "0123456789abcdef";

static void print_int(unsigned long long val, unsigned base) {
    char  buf[32];
    char *p = buf;

    do {
        *p++ = digits[val % base];
    } while (val /= base);

    while (p > buf) {
        printf_putc(*--p);
    }
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p == '\n') {
            printf_putc('\r');
        }
        if (*p != '%') {
            printf_putc(*p);
            continue;
        }

        p++;
        int wide = 0;
        if (*p == 'l') {
            wide = 1;
            p++;
        }
        switch (*p) {
        case 'd': {
            long long val = wide ? va_arg(args, long) : va_arg(args, int);
            if (val < 0) {
                printf_putc('-');
                val = -val;
            }
            print_int((unsigned long long)val, 10);
            break;
        }
        case 'u':
            print_int(wide ? va_arg(args, unsigned long)
                           : va_arg(args, unsigned int),
                      10);
            break;
        case 'x':
            print_int(wide ? va_arg(args, unsigned long)
                           : va_arg(args, unsigned int),
                      16);
            break;
        case 'p':
            printf_putc('0');
            printf_putc('x');
            print_int((uintptr_t)va_arg(args, void *), 16);
            break;
        case 'c':
            printf_putc((char)va_arg(args, int));
            break;
        case 's':
            for (const char *s = va_arg(args, const char *); *s; s++) {
                printf_putc(*s);
            }
            break;
        case '%':
            printf_putc('%');
            break;
        }
    }
    va_end(args);
}
