#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>

int main() {
    int n;
    volatile double zero = 0;
    printf("[%d] [%5d] [%-5d] [%05d] [%+d] [% d]\n", 42, 42, 42, 42, 42, 42);
    printf("[%x] [%#X] [%#o] [%b] [%.3d] [%.0d]\n", 255, 255, 8, 5, 7, 0);
    printf("[%hhd] [%hu] [%ld] [%zu] [%jd]\n", 300, 70000, -1L, sizeof(long),
           (intmax_t)1 << 40);
    printf("[%s] [%.3s] [%8s] [%-8s] [%c]%n\n", "text", "text", "right", "left",
           'c', &n);
    printf("that line was %d bytes before its newline\n", n);
    printf("[%f] [%.2f] [%e] [%g] [%g]\n", 0.1, 2.675, 1234.5, 0.0001, 1e20);
    printf("[%.17g] [%.0f] [%.0f] [%a]\n", 0.1, 0.5, 1.5, 1.0);
    printf("[%g] [%g] [%.0f] [%Lf]\n", 1e300 * 1e10, zero / zero, 1e23,
           1.0L / 3);
    printf("[%.3g] [%g] [%#g] [%08.3f]\n", 5e-324, 4.9406564584124654e-324, 1.0,
           -3.14159);
    printf("[%2$s %1$s]\n", "world", "hello");
    printf("[%*d] [%-*d]\n", 6, 1, 6, 2);
    setlocale(LC_ALL, "C.UTF-8");
    printf("[%ls] [%lc] [%.4ls]\n", L"日本語", L'語', L"日本語");
    errno = ENOENT;
    printf("[%m]\n");
    char buf[8];
    int want = snprintf(buf, sizeof buf, "%s", "a long string");
    printf("[%s] wanted %d\n", buf, want);
    return 0;
}
