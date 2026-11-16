#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_wint_t
#define __NEED_wctype_t
#define __NEED_locale_t
#include <bits/alltypes.h>

#define WEOF 0xffffffffU

typedef int wctrans_t;

int iswalnum(wint_t c);
int iswalpha(wint_t c);
int iswblank(wint_t c);
int iswcntrl(wint_t c);
int iswdigit(wint_t c);
int iswgraph(wint_t c);
int iswlower(wint_t c);
int iswprint(wint_t c);
int iswpunct(wint_t c);
int iswspace(wint_t c);
int iswupper(wint_t c);
int iswxdigit(wint_t c);
int iswctype(wint_t c, wctype_t t);
wctype_t wctype(const char *name);
wint_t towlower(wint_t c);
wint_t towupper(wint_t c);
wctrans_t wctrans(const char *name);
wint_t towctrans(wint_t c, wctrans_t t);

int iswalnum_l(wint_t c, locale_t loc);
int iswalpha_l(wint_t c, locale_t loc);
int iswblank_l(wint_t c, locale_t loc);
int iswcntrl_l(wint_t c, locale_t loc);
int iswdigit_l(wint_t c, locale_t loc);
int iswgraph_l(wint_t c, locale_t loc);
int iswlower_l(wint_t c, locale_t loc);
int iswprint_l(wint_t c, locale_t loc);
int iswpunct_l(wint_t c, locale_t loc);
int iswspace_l(wint_t c, locale_t loc);
int iswupper_l(wint_t c, locale_t loc);
int iswxdigit_l(wint_t c, locale_t loc);
int iswctype_l(wint_t c, wctype_t t, locale_t loc);
wctype_t wctype_l(const char *name, locale_t loc);
wint_t towlower_l(wint_t c, locale_t loc);
wint_t towupper_l(wint_t c, locale_t loc);
wctrans_t wctrans_l(const char *name, locale_t loc);
wint_t towctrans_l(wint_t c, wctrans_t t, locale_t loc);

#ifdef __cplusplus
}
#endif
