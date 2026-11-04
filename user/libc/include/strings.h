#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_locale_t
#include <bits/alltypes.h>

int ffs(int i);
int ffsl(long i);
int ffsll(long long i);

int strcasecmp(const char *a, const char *b);
int strcasecmp_l(const char *a, const char *b, locale_t loc);
int strncasecmp(const char *a, const char *b, size_t n);
int strncasecmp_l(const char *a, const char *b, size_t n, locale_t loc);

#ifdef __cplusplus
}
#endif
