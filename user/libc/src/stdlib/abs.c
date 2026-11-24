#include <inttypes.h>
#include <stdlib.h>

int abs(int i) { return (i < 0) ? -i : i; }
long labs(long i) { return (i < 0) ? -i : i; }
long long llabs(long long i) { return (i < 0) ? -i : i; }
intmax_t imaxabs(intmax_t i) { return (i < 0) ? -i : i; }

div_t div(int n, int d) { return (div_t){n / d, n % d}; }
ldiv_t ldiv(long n, long d) { return (ldiv_t){n / d, n % d}; }
lldiv_t lldiv(long long n, long long d) { return (lldiv_t){n / d, n % d}; }
imaxdiv_t imaxdiv(intmax_t n, intmax_t d) { return (imaxdiv_t){n / d, n % d}; }
