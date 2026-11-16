#include <ctype.h>

int isalnum_l(int c, locale_t loc) {
    (void)loc;
    return isalnum(c);
}

int isalpha_l(int c, locale_t loc) {
    (void)loc;
    return isalpha(c);
}

int isblank_l(int c, locale_t loc) {
    (void)loc;
    return isblank(c);
}

int iscntrl_l(int c, locale_t loc) {
    (void)loc;
    return iscntrl(c);
}

int isdigit_l(int c, locale_t loc) {
    (void)loc;
    return isdigit(c);
}

int isgraph_l(int c, locale_t loc) {
    (void)loc;
    return isgraph(c);
}

int islower_l(int c, locale_t loc) {
    (void)loc;
    return islower(c);
}

int isprint_l(int c, locale_t loc) {
    (void)loc;
    return isprint(c);
}

int ispunct_l(int c, locale_t loc) {
    (void)loc;
    return ispunct(c);
}

int isspace_l(int c, locale_t loc) {
    (void)loc;
    return isspace(c);
}

int isupper_l(int c, locale_t loc) {
    (void)loc;
    return isupper(c);
}

int isxdigit_l(int c, locale_t loc) {
    (void)loc;
    return isxdigit(c);
}

int tolower_l(int c, locale_t loc) {
    (void)loc;
    return tolower(c);
}

int toupper_l(int c, locale_t loc) {
    (void)loc;
    return toupper(c);
}
