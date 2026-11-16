#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_mbstate_t
#include <bits/alltypes.h>

#ifndef __cplusplus
typedef __UINT_LEAST16_TYPE__ char16_t;
typedef __UINT_LEAST32_TYPE__ char32_t;
#endif

size_t mbrtoc16(char16_t *__restrict pc, const char *__restrict s, size_t n,
                mbstate_t *__restrict ps);
size_t c16rtomb(char *__restrict s, char16_t c, mbstate_t *__restrict ps);
size_t mbrtoc32(char32_t *__restrict pc, const char *__restrict s, size_t n,
                mbstate_t *__restrict ps);
size_t c32rtomb(char *__restrict s, char32_t c, mbstate_t *__restrict ps);

#ifdef __cplusplus
}
#endif
