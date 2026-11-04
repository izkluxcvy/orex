#include <errno.h>
#include <string.h>
#include <unistd.h>

size_t confstr(int name, char *buf, size_t len) {
    const char *v;
    switch (name) {
    case _CS_PATH:
        v = "/bin:/usr/bin";
        break;
    case _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS:
        v = "POSIX_V8_LP64_OFF64";
        break;
    case _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS:
        v = "POSIX_V7_LP64_OFF64";
        break;
    case _CS_V8_ENV:
    case _CS_V7_ENV:
    case _CS_POSIX_V8_LP64_OFF64_CFLAGS:
    case _CS_POSIX_V8_LP64_OFF64_LDFLAGS:
    case _CS_POSIX_V8_LP64_OFF64_LIBS:
    case _CS_POSIX_V7_LP64_OFF64_CFLAGS:
    case _CS_POSIX_V7_LP64_OFF64_LDFLAGS:
    case _CS_POSIX_V7_LP64_OFF64_LIBS:
        v = "";
        break;
    default:
        errno = EINVAL;
        return 0;
    }
    if (len) {
        strlcpy(buf, v, len);
    }
    return strlen(v) + 1;
}
