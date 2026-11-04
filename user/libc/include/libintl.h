#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_locale_t
#include <bits/alltypes.h>

#define TEXTDOMAIN_MAX 255

char *gettext(const char *msgid);
char *dgettext(const char *domain, const char *msgid);
char *dcgettext(const char *domain, const char *msgid, int category);
char *ngettext(const char *msgid, const char *plural, unsigned long n);
char *dngettext(const char *domain, const char *msgid, const char *plural,
                unsigned long n);
char *dcngettext(const char *domain, const char *msgid, const char *plural,
                 unsigned long n, int category);
char *gettext_l(const char *msgid, locale_t loc);
char *dgettext_l(const char *domain, const char *msgid, locale_t loc);
char *dcgettext_l(const char *domain, const char *msgid, int category,
                  locale_t loc);
char *ngettext_l(const char *msgid, const char *plural, unsigned long n,
                 locale_t loc);
char *dngettext_l(const char *domain, const char *msgid, const char *plural,
                  unsigned long n, locale_t loc);
char *dcngettext_l(const char *domain, const char *msgid, const char *plural,
                   unsigned long n, int category, locale_t loc);
char *textdomain(const char *domain);
char *bindtextdomain(const char *domain, const char *dir);
char *bind_textdomain_codeset(const char *domain, const char *codeset);

#ifdef __cplusplus
}
#endif
