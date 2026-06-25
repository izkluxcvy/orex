#include "printf.h"

void (*printf_putc)(char c);
static void print_int(unsigned long long val, int base);

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (char *p = (char *)fmt; *p != '\0'; p++) {
        if (*p != '%') {
            printf_putc(*p);
            continue;
        }

        p++;
        switch (*p) {
        case 'd':
            int val = va_arg(args, int);
            if (val < 0) {
                printf_putc('-');
                val = -val;
            }
            print_int(val, 10);
            break;
        case 'u':
            val = va_arg(args, unsigned int);
            print_int(val, 10);
            break;
        case 'x':
            val = va_arg(args, int);
            print_int(val, 16);
            break;
        case 'p':
            val = (unsigned long long)va_arg(args, void *);
            print_int(val, 16);
            break;
        }
    }
}

const char *digits = "0123456789abcdef";

static void print_int(unsigned long long val, int base) {
    char  buf[32];
    char *p = buf;

    do {
        *p++ = digits[val % base];
    } while (val /= base);

    while (p > buf) {
        printf_putc(*--p);
    }
}
