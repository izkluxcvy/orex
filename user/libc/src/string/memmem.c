#include <string.h>

void *memmem(const void *hay, size_t hlen, const void *needle, size_t nlen) {
    const unsigned char *h = hay;
    if (!nlen) {
        return (void *)h;
    }
    for (; hlen >= nlen; h++, hlen--) {
        if (*h == *(const unsigned char *)needle && !memcmp(h, needle, nlen)) {
            return (void *)h;
        }
    }
    return nullptr;
}
