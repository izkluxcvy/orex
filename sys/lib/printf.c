#include <stdarg.h>

#include <printf.h>

static void (*sinks[PRINTF_MAX_SINKS])(char c);
static int sink_count;

void printf_add_sink(void (*putc)(char c)) {
    if (sink_count < PRINTF_MAX_SINKS) {
        sinks[sink_count++] = putc;
    }
}

static void emit(char c) {
    for (int i = 0; i < sink_count; i++) {
        sinks[i](c);
    }
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
    for (char *p = (char *)fmt; *p; p++) {
        if (*p == '\n') {
            emit('\r');
        }
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
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
