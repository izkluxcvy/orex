#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_wchar_t
#define __NEED_wint_t
#define __NEED_mbstate_t
#define __NEED_locale_t
#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>
#define __need_va_list
#include <stdarg.h>

typedef struct __FILE FILE;

struct tm;

#define WEOF      0xffffffffU
#define WCHAR_MIN (-2147483647 - 1)
#define WCHAR_MAX 2147483647

wint_t btowc(int c);
int wctob(wint_t wc);
int mbsinit(const mbstate_t *ps);
size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict ps);
size_t mbrtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n,
               mbstate_t *__restrict ps);
size_t wcrtomb(char *__restrict s, wchar_t wc, mbstate_t *__restrict ps);
size_t mbsrtowcs(wchar_t *__restrict dst, const char **__restrict src,
                 size_t len, mbstate_t *__restrict ps);
size_t mbsnrtowcs(wchar_t *__restrict dst, const char **__restrict src,
                  size_t nms, size_t len, mbstate_t *__restrict ps);
size_t wcsrtombs(char *__restrict dst, const wchar_t **__restrict src,
                 size_t len, mbstate_t *__restrict ps);
size_t wcsnrtombs(char *__restrict dst, const wchar_t **__restrict src,
                  size_t nwc, size_t len, mbstate_t *__restrict ps);

wchar_t *wcscpy(wchar_t *__restrict d, const wchar_t *__restrict s);
wchar_t *wcsncpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n);
wchar_t *wcpcpy(wchar_t *__restrict d, const wchar_t *__restrict s);
wchar_t *wcpncpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n);
wchar_t *wcscat(wchar_t *__restrict d, const wchar_t *__restrict s);
wchar_t *wcsncat(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n);
size_t wcslcpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n);
size_t wcslcat(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n);
int wcscmp(const wchar_t *a, const wchar_t *b);
int wcsncmp(const wchar_t *a, const wchar_t *b, size_t n);
int wcscasecmp(const wchar_t *a, const wchar_t *b);
int wcsncasecmp(const wchar_t *a, const wchar_t *b, size_t n);
int wcscasecmp_l(const wchar_t *a, const wchar_t *b, locale_t loc);
int wcsncasecmp_l(const wchar_t *a, const wchar_t *b, size_t n, locale_t loc);
int wcscoll(const wchar_t *a, const wchar_t *b);
int wcscoll_l(const wchar_t *a, const wchar_t *b, locale_t loc);
size_t wcsxfrm(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n);
size_t wcsxfrm_l(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n,
                 locale_t loc);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
size_t wcsspn(const wchar_t *s, const wchar_t *set);
size_t wcscspn(const wchar_t *s, const wchar_t *set);
wchar_t *wcspbrk(const wchar_t *s, const wchar_t *set);
wchar_t *wcsstr(const wchar_t *__restrict h, const wchar_t *__restrict n);
wchar_t *wcstok(wchar_t *__restrict s, const wchar_t *__restrict sep,
                wchar_t **__restrict save);
size_t wcslen(const wchar_t *s);
size_t wcsnlen(const wchar_t *s, size_t n);
wchar_t *wcsdup(const wchar_t *s);
int wcwidth(wchar_t c);
int wcswidth(const wchar_t *s, size_t n);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);
int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n);
wchar_t *wmemcpy(wchar_t *__restrict d, const wchar_t *__restrict s, size_t n);
wchar_t *wmemmove(wchar_t *d, const wchar_t *s, size_t n);
wchar_t *wmemset(wchar_t *d, wchar_t c, size_t n);

double wcstod(const wchar_t *__restrict s, wchar_t **__restrict end);
float wcstof(const wchar_t *__restrict s, wchar_t **__restrict end);
long double wcstold(const wchar_t *__restrict s, wchar_t **__restrict end);
long wcstol(const wchar_t *__restrict s, wchar_t **__restrict end, int base);
long long wcstoll(const wchar_t *__restrict s, wchar_t **__restrict end,
                  int base);
unsigned long wcstoul(const wchar_t *__restrict s, wchar_t **__restrict end,
                      int base);
unsigned long long wcstoull(const wchar_t *__restrict s,
                            wchar_t **__restrict end, int base);

wint_t fgetwc(FILE *f);
wint_t getwc(FILE *f);
wint_t getwchar(void);
wint_t fputwc(wchar_t c, FILE *f);
wint_t putwc(wchar_t c, FILE *f);
wint_t putwchar(wchar_t c);
wchar_t *fgetws(wchar_t *__restrict s, int n, FILE *__restrict f);
int fputws(const wchar_t *__restrict s, FILE *__restrict f);
wint_t ungetwc(wint_t c, FILE *f);
int fwide(FILE *f, int mode);
FILE *open_wmemstream(wchar_t **buf, size_t *size);

int fwprintf(FILE *__restrict f, const wchar_t *__restrict fmt, ...);
int swprintf(wchar_t *__restrict s, size_t n, const wchar_t *__restrict fmt,
             ...);
int wprintf(const wchar_t *__restrict fmt, ...);
int vfwprintf(FILE *__restrict f, const wchar_t *__restrict fmt, va_list ap);
int vswprintf(wchar_t *__restrict s, size_t n, const wchar_t *__restrict fmt,
              va_list ap);
int vwprintf(const wchar_t *__restrict fmt, va_list ap);
int fwscanf(FILE *__restrict f, const wchar_t *__restrict fmt, ...);
int swscanf(const wchar_t *__restrict s, const wchar_t *__restrict fmt, ...);
int wscanf(const wchar_t *__restrict fmt, ...);
int vfwscanf(FILE *__restrict f, const wchar_t *__restrict fmt, va_list ap);
int vswscanf(const wchar_t *__restrict s, const wchar_t *__restrict fmt,
             va_list ap);
int vwscanf(const wchar_t *__restrict fmt, va_list ap);

size_t wcsftime(wchar_t *__restrict s, size_t n, const wchar_t *__restrict fmt,
                const struct tm *__restrict tm);

#ifdef __cplusplus
}
#endif
