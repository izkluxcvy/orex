#include <string.h>

int strcoll(const char *a, const char *b) { return strcmp(a, b); }

int strcoll_l(const char *a, const char *b, locale_t loc) {
    (void)loc;
    return strcmp(a, b);
}
