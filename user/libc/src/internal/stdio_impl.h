#pragma once

#include <stdio.h>
#include <sys/types.h>
#include <wchar.h>

#include <pthread_impl.h>

#define UNGET 8

// f->flags
#define F_NORD   0x001
#define F_NOWR   0x002
#define F_EOF    0x004
#define F_ERR    0x008
#define F_APPEND 0x010
#define F_NOBUF  0x020
#define F_LBF    0x040
#define F_BUFSET 0x080
#define F_OWNBUF 0x100
#define F_PERM   0x200

struct __FILE {
    unsigned flags;
    unsigned char *rpos, *rend;
    unsigned char *wbase, *wpos, *wend;
    unsigned char *buf;
    size_t bufsize;
    int fd;
    int lbf;
    int orientation;

    ssize_t (*read)(FILE *f, unsigned char *buf, size_t n);
    ssize_t (*write)(FILE *f, const unsigned char *buf, size_t n);
    off_t (*seek)(FILE *f, off_t off, int whence);
    int (*close)(FILE *f);
    void *cookie;

    FILE *prev, *next;
    volatile int lock;
    volatile int lock_owner;
    int lock_count;
    pid_t pipe_pid;
    unsigned char tiny[UNGET + 1];
};

int __lockfile(FILE *f);
void __unlockfile(FILE *f);
#define FLOCK(f) int __need_unlock = __libc_threads.threaded ? __lockfile(f) : 0
#define FUNLOCK(f)                                                             \
    do {                                                                       \
        if (__need_unlock) {                                                   \
            __unlockfile(f);                                                   \
        }                                                                      \
    } while (0)

void __ofl_lock(void);
void __ofl_unlock(void);

FILE *__file_new(int fd, unsigned flags);
void __file_link(FILE *f);
void __file_unlink(FILE *f);
int __open_flags(const char *mode, int *oflags);

int __toread(FILE *f);
int __towrite(FILE *f);
int __uflow(FILE *f);
int __overflow(FILE *f, int c);
int __flush(FILE *f);

ssize_t __fd_read(FILE *f, unsigned char *buf, size_t n);
ssize_t __fd_write(FILE *f, const unsigned char *buf, size_t n);
off_t __fd_seek(FILE *f, off_t off, int whence);
int __fd_close(FILE *f);

struct sink {
    int (*write)(struct sink *s, const char *p, size_t n);
    int (*wwrite)(struct sink *s, const wchar_t *p, size_t n);
    size_t count;
    void *ctx;
};

struct source {
    int (*get)(struct source *s);
    void (*unget)(struct source *s, int c);
    size_t count;
    int eof;
    void *ctx;
};

int __vprintf_core(struct sink *out, const char *fmt, va_list ap);
int __vwprintf_core(struct sink *out, const wchar_t *fmt, va_list ap);
int __vscanf_core(struct source *in, const char *fmt, va_list ap);
int __vwscanf_core(struct source *in, const wchar_t *fmt, va_list ap);

struct float_kind {
    int bits, emin, emax;
};
extern const struct float_kind __float_f, __float_d, __float_ld;
long double __floatscan(const char *s, char **end, const struct float_kind *k);
