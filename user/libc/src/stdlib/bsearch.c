#include <stdlib.h>

void *bsearch(const void *key, const void *base, size_t n, size_t size,
              int (*cmp)(const void *, const void *)) {
    const char *lo = base;
    while (n) {
        const char *mid = lo + n / 2 * size;
        int c = cmp(key, mid);
        if (c == 0) {
            return (void *)mid;
        }
        if (c > 0) {
            lo = mid + size;
            n -= n / 2 + 1;
        } else {
            n /= 2;
        }
    }
    return nullptr;
}
