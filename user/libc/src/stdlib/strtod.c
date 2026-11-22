#include <stdlib.h>

#include <stdio_impl.h>

float strtof(const char *__restrict s, char **__restrict end) {
    return (float)__floatscan(s, end, &__float_f);
}

double strtod(const char *__restrict s, char **__restrict end) {
    return (double)__floatscan(s, end, &__float_d);
}

long double strtold(const char *__restrict s, char **__restrict end) {
    return __floatscan(s, end, &__float_ld);
}

double atof(const char *s) { return strtod(s, nullptr); }
