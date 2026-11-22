#include <stdarg.h>

#include <stdio_impl.h>

static int file_get(struct source *s) { return getc(s->ctx); }
static void file_unget(struct source *s, int c) { ungetc(c, s->ctx); }

int vfscanf(FILE *__restrict f, const char *__restrict fmt, va_list ap) {
    struct source s = {.get = file_get, .unget = file_unget, .ctx = f};
    FLOCK(f);
    int r = __vscanf_core(&s, fmt, ap);
    FUNLOCK(f);
    return r;
}

static int str_get(struct source *s) {
    const unsigned char **p = s->ctx;
    return **p ? *(*p)++ : EOF;
}

static void str_unget(struct source *s, int c) {
    (void)c;
    const unsigned char **p = s->ctx;
    (*p)--;
}

int vsscanf(const char *__restrict str, const char *__restrict fmt,
            va_list ap) {
    const unsigned char *p = (const unsigned char *)str;
    struct source s = {.get = str_get, .unget = str_unget, .ctx = &p};
    return __vscanf_core(&s, fmt, ap);
}

int vscanf(const char *__restrict fmt, va_list ap) {
    return vfscanf(stdin, fmt, ap);
}

int fscanf(FILE *__restrict f, const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfscanf(f, fmt, ap);
    va_end(ap);
    return r;
}

int scanf(const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vscanf(fmt, ap);
    va_end(ap);
    return r;
}

int sscanf(const char *__restrict str, const char *__restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(str, fmt, ap);
    va_end(ap);
    return r;
}
