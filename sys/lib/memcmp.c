#include <stddef.h>
#include <stdint.h>

#include <memcmp.h>

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = a, *q = b;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != q[i]) {
            return p[i] - q[i];
        }
    }
    return 0;
}
