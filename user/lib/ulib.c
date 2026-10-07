#include "ulib.h"

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++, b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strchr(const char *s, int c) {
    for (;; s++) {
        if (*s == (char)c) {
            return (char *)s;
        }
        if (!*s) {
            return nullptr;
        }
    }
}

void *memset(void *d, int c, size_t n) {
    unsigned char *p = d;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return d;
}

void *memcpy(void *d, const void *s, size_t n) {
    unsigned char       *p = d;
    const unsigned char *q = s;
    for (size_t i = 0; i < n; i++) {
        p[i] = q[i];
    }
    return d;
}

void puts(const char *s) { write(1, s, strlen(s)); }

void putnum(long v) {
    char          buf[24];
    char         *p   = buf + sizeof(buf);
    int           neg = v < 0;
    unsigned long u   = neg ? -(unsigned long)v : (unsigned long)v;
    do {
        *--p = (char)('0' + (u % 10));
    } while (u /= 10);
    if (neg) {
        *--p = '-';
    }
    write(1, p, (buf + sizeof(buf)) - p);
}

#define SA_RESTORER 0x0400'0000

struct k_sigaction {
    sighandler_t  handler;
    unsigned long flags;
    void (*restorer)(void);
    unsigned long mask;
};

__attribute__((naked)) static void restore_rt() {
    __asm__ __volatile__("mov eax, 15\n\t"
                         "syscall");
}

sighandler_t signal(int sig, sighandler_t handler) {
    struct k_sigaction act = {
        .handler = handler, .flags = SA_RESTORER, .restorer = restore_rt};
    struct k_sigaction old;
    if (syscall4(SYS_rt_sigaction, sig, (long)&act, (long)&old, 8) < 0) {
        return (sighandler_t)-1;
    }
    return old.handler;
}

long strtol(const char *s, char **end, int base) {
    int neg = *s == '-';
    s += neg || *s == '+';
    long v = 0;
    for (;; s++) {
        int d = *s >= '0' && *s <= '9'   ? *s - '0'
                : *s >= 'a' && *s <= 'z' ? *s - 'a' + 10
                                         : 99;
        if (d >= base) {
            break;
        }
        v = v * base + d;
    }
    if (end) {
        *end = (char *)s;
    }
    return neg ? -v : v;
}
