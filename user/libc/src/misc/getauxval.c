#include <errno.h>
#include <sys/auxv.h>

#include <libc.h>

static const unsigned long *find(unsigned long type) {
    for (const unsigned long *p = __auxv; *p != AT_NULL; p += 2) {
        if (*p == type) {
            return p;
        }
    }
    return nullptr;
}

unsigned long __getauxval(unsigned long type) {
    const unsigned long *p = find(type);
    return p ? p[1] : 0;
}

unsigned long getauxval(unsigned long type) {
    const unsigned long *p = find(type);
    if (!p) {
        errno = ENOENT;
        return 0;
    }
    return p[1];
}
