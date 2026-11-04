#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_wchar_t
#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>

#include <bits/waitstatus.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX     2147483647
#define MB_CUR_MAX   (__ctype_get_mb_cur_max()) // 4 in a UTF-8 locale

typedef struct {
    int quot, rem;
} div_t;
typedef struct {
    long quot, rem;
} ldiv_t;
typedef struct {
    long long quot, rem;
} lldiv_t;

void *aligned_alloc(size_t align, size_t n);
void *calloc(size_t n, size_t size);
void free(void *p);
void free_aligned_sized(void *p, size_t align, size_t n);
void free_sized(void *p, size_t n);
void *malloc(size_t n);
int posix_memalign(void **out, size_t align, size_t n);
void *realloc(void *p, size_t n);
void *reallocarray(void *p, size_t n, size_t size);

__attribute__((__noreturn__)) void abort(void);
__attribute__((__noreturn__)) void exit(int status);
__attribute__((__noreturn__)) void _Exit(int status);
__attribute__((__noreturn__)) void quick_exit(int status);
int atexit(void (*fn)(void));
int at_quick_exit(void (*fn)(void));

double atof(const char *s);
double strtod(const char *__restrict s, char **__restrict end);
float strtof(const char *__restrict s, char **__restrict end);
long double strtold(const char *__restrict s, char **__restrict end);
int atoi(const char *s);
long atol(const char *s);
long long atoll(const char *s);
long strtol(const char *__restrict s, char **__restrict end, int base);
long long strtoll(const char *__restrict s, char **__restrict end, int base);
unsigned long strtoul(const char *__restrict s, char **__restrict end,
                      int base);
unsigned long long strtoull(const char *__restrict s, char **__restrict end,
                            int base);

char *getenv(const char *name);
char *secure_getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
void setkey(const char *key);
int unsetenv(const char *name);
int putenv(char *string);
int clearenv(void); // not POSIX

void *bsearch(const void *key, const void *base, size_t n, size_t size,
              int (*cmp)(const void *, const void *));
void qsort(void *base, size_t n, size_t size,
           int (*cmp)(const void *, const void *));
void qsort_r(void *base, size_t n, size_t size,
             int (*cmp)(const void *, const void *, void *), void *arg);

int abs(int i);
long labs(long i);
long long llabs(long long i);
div_t div(int n, int d);
ldiv_t ldiv(long n, long d);
lldiv_t lldiv(long long n, long long d);

int rand(void);
int rand_r(unsigned *seed);
void srand(unsigned seed);
long random(void);
void srandom(unsigned seed);
char *initstate(unsigned seed, char *state, size_t size);
char *setstate(char *state);
double drand48(void);
double erand48(unsigned short xsubi[3]);
long jrand48(unsigned short xsubi[3]);
void lcong48(unsigned short param[7]);
long lrand48(void);
long mrand48(void);
long nrand48(unsigned short xsubi[3]);
unsigned short *seed48(unsigned short seed16v[3]);
void srand48(long seed);

size_t __ctype_get_mb_cur_max(void);
int mblen(const char *s, size_t n);
int mbtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n);
int wctomb(char *s, wchar_t wc);
size_t mbstowcs(wchar_t *__restrict dst, const char *__restrict src, size_t n);
size_t wcstombs(char *__restrict dst, const wchar_t *__restrict src, size_t n);

char *mkdtemp(char *tmpl);
int mkostemp(char *tmpl, int flags);
int mkstemp(char *tmpl);
int system(const char *cmd);
int grantpt(int fd);
int posix_openpt(int flags);
char *ptsname(int fd);
int ptsname_r(int fd, char *buf, size_t size);
int unlockpt(int fd);
char *realpath(const char *__restrict path, char *__restrict resolved);
int getsubopt(char **optionp, char *const *tokens, char **valuep);
long a64l(const char *s);
char *l64a(long v);

#ifdef __cplusplus
}
#endif
