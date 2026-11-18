#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_off_t
#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>
#define __need_va_list
#include <stdarg.h>

typedef struct __FILE FILE;

typedef struct {
    off_t __pos;
} fpos_t;

#define BUFSIZ       8192
#define EOF          (-1)
#define FILENAME_MAX 256
#define FOPEN_MAX    16
#define L_ctermid    20
#define L_tmpnam     20
#define P_tmpdir     "/tmp"
#define TMP_MAX      238328

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;
#define stdin  (stdin)
#define stdout (stdout)
#define stderr (stderr)

void clearerr(FILE *f);
char *ctermid(char *s);
int fclose(FILE *f);
FILE *fdopen(int fd, const char *mode);
int feof(FILE *f);
int ferror(FILE *f);
int fflush(FILE *f);
int fgetc(FILE *f);
int fgetpos(FILE *__restrict f, fpos_t *__restrict pos);
char *fgets(char *__restrict s, int n, FILE *__restrict f);
int fileno(FILE *f);
void flockfile(FILE *f);
FILE *fmemopen(void *__restrict buf, size_t size, const char *__restrict mode);
FILE *fopen(const char *__restrict path, const char *__restrict mode);
int fputc(int c, FILE *f);
int fputs(const char *__restrict s, FILE *__restrict f);
size_t fread(void *__restrict p, size_t size, size_t n, FILE *__restrict f);
FILE *freopen(const char *__restrict path, const char *__restrict mode,
              FILE *__restrict f);
int fseek(FILE *f, long off, int whence);
int fseeko(FILE *f, off_t off, int whence);
int fsetpos(FILE *f, const fpos_t *pos);
long ftell(FILE *f);
off_t ftello(FILE *f);
int ftrylockfile(FILE *f);
void funlockfile(FILE *f);
size_t fwrite(const void *__restrict p, size_t size, size_t n,
              FILE *__restrict f);
int getc(FILE *f);
int getchar(void);
int getc_unlocked(FILE *f);
int getchar_unlocked(void);
ssize_t getdelim(char **__restrict line, size_t *__restrict n, int delim,
                 FILE *__restrict f);
ssize_t getline(char **__restrict line, size_t *__restrict n,
                FILE *__restrict f);
FILE *open_memstream(char **buf, size_t *size);
int pclose(FILE *f);
void perror(const char *s);
FILE *popen(const char *cmd, const char *mode);
int putc(int c, FILE *f);
int putchar(int c);
int putc_unlocked(int c, FILE *f);
int putchar_unlocked(int c);
int puts(const char *s);
int remove(const char *path);
int rename(const char *from, const char *to);
int renameat(int fromdir, const char *from, int todir, const char *to);
void rewind(FILE *f);
void setbuf(FILE *__restrict f, char *__restrict buf);
int setvbuf(FILE *__restrict f, char *__restrict buf, int mode, size_t size);
FILE *tmpfile(void);
char *tmpnam(char *s);
char *tempnam(const char *dir, const char *pfx);
int ungetc(int c, FILE *f);

int asprintf(char **__restrict out, const char *__restrict fmt, ...);
int dprintf(int fd, const char *__restrict fmt, ...);
int fprintf(FILE *__restrict f, const char *__restrict fmt, ...);
int printf(const char *__restrict fmt, ...);
int snprintf(char *__restrict s, size_t n, const char *__restrict fmt, ...);
int sprintf(char *__restrict s, const char *__restrict fmt, ...);
int vasprintf(char **__restrict out, const char *__restrict fmt, va_list ap);
int vdprintf(int fd, const char *__restrict fmt, va_list ap);
int vfprintf(FILE *__restrict f, const char *__restrict fmt, va_list ap);
int vprintf(const char *__restrict fmt, va_list ap);
int vsnprintf(char *__restrict s, size_t n, const char *__restrict fmt,
              va_list ap);
int vsprintf(char *__restrict s, const char *__restrict fmt, va_list ap);

int fscanf(FILE *__restrict f, const char *__restrict fmt, ...);
int scanf(const char *__restrict fmt, ...);
int sscanf(const char *__restrict s, const char *__restrict fmt, ...);
int vfscanf(FILE *__restrict f, const char *__restrict fmt, va_list ap);
int vscanf(const char *__restrict fmt, va_list ap);
int vsscanf(const char *__restrict s, const char *__restrict fmt, va_list ap);

#ifdef __cplusplus
}
#endif
