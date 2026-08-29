#include <stddef.h>
#include <stdint.h>

void memset(void *dest, int value, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((uint8_t *)dest)[i] = (uint8_t)value;
    }
}
