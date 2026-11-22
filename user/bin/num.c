#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        char *end;
        errno = 0;
        long l = strtol(argv[i], &end, 0);
        printf("%s: strtol %ld, rest \"%s\"%s\n", argv[i], l, end,
               errno ? " (ERANGE)" : "");
        errno = 0;
        double d = strtod(argv[i], &end);
        printf("%s: strtod %.17g = %a, rest \"%s\"%s\n", argv[i], d, d, end,
               errno ? " (ERANGE)" : "");
    }
    if (argc > 1) {
        return 0;
    }

    double sum = 0, x;
    int n = 0, r;
    while ((r = scanf("%lf", &x)) == 1) {
        sum += x;
        n++;
    }
    if (r == 0) {
        char word[32];
        scanf("%31s", word);
        printf("not a number: %s\n", word);
    }
    printf("%d numbers, sum %g\n", n, sum);
    return 0;
}
