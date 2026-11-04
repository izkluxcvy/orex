#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_locale_t
#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>

void *memccpy(void *__restrict dst, const void *__restrict src, int c,
              size_t n);
void *memchr(const void *s, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memcpy(void *__restrict dst, const void *__restrict src, size_t n);
void *memmem(const void *hay, size_t hlen, const void *needle, size_t nlen);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
void *memset_explicit(void *dst, int c, size_t n);

char *stpcpy(char *__restrict dst, const char *__restrict src);
char *stpncpy(char *__restrict dst, const char *__restrict src, size_t n);
char *strcat(char *__restrict dst, const char *__restrict src);
char *strchr(const char *s, int c);
int strcmp(const char *a, const char *b);
int strcoll(const char *a, const char *b);
int strcoll_l(const char *a, const char *b, locale_t loc);
char *strcpy(char *__restrict dst, const char *__restrict src);
char *strdup(const char *s);
size_t strcspn(const char *s, const char *reject);
char *strerror(int err);
char *strerror_l(int err, locale_t loc);
int strerror_r(int err, char *buf, size_t n);
size_t strlcat(char *__restrict dst, const char *__restrict src, size_t n);
size_t strlcpy(char *__restrict dst, const char *__restrict src, size_t n);
size_t strlen(const char *s);
char *strncat(char *__restrict dst, const char *__restrict src, size_t n);
int strncmp(const char *a, const char *b, size_t n);
char *strncpy(char *__restrict dst, const char *__restrict src, size_t n);
char *strndup(const char *s, size_t max);
size_t strnlen(const char *s, size_t max);
char *strpbrk(const char *s, const char *accept);
char *strrchr(const char *s, int c);
char *strsignal(int sig);
size_t strspn(const char *s, const char *accept);
char *strstr(const char *hay, const char *needle);
char *strtok(char *__restrict s, const char *__restrict sep);
char *strtok_r(char *__restrict s, const char *__restrict sep,
               char **__restrict save);
size_t strxfrm(char *__restrict dst, const char *__restrict src, size_t n);
size_t strxfrm_l(char *__restrict dst, const char *__restrict src, size_t n,
                 locale_t loc);

#ifdef __cplusplus
}
#endif
