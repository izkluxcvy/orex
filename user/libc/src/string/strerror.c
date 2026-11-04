#include <errno.h>
#include <string.h>

#include "fmt.h"

static const char *const messages[] = {
    [0] = "Success",
    [EPERM] = "Operation not permitted",
    [ENOENT] = "No such file or directory",
    [ESRCH] = "No such process",
    [EINTR] = "Interrupted system call",
    [EIO] = "Input/output error",
    [ENXIO] = "No such device or address",
    [E2BIG] = "Argument list too long",
    [ENOEXEC] = "Exec format error",
    [EBADF] = "Bad file descriptor",
    [ECHILD] = "No child processes",
    [EAGAIN] = "Resource temporarily unavailable",
    [ENOMEM] = "Cannot allocate memory",
    [EACCES] = "Permission denied",
    [EFAULT] = "Bad address",
    [ENOTBLK] = "Block device required",
    [EBUSY] = "Device or resource busy",
    [EEXIST] = "File exists",
    [EXDEV] = "Invalid cross-device link",
    [ENODEV] = "No such device",
    [ENOTDIR] = "Not a directory",
    [EISDIR] = "Is a directory",
    [EINVAL] = "Invalid argument",
    [ENFILE] = "Too many open files in system",
    [EMFILE] = "Too many open files",
    [ENOTTY] = "Inappropriate ioctl for device",
    [ETXTBSY] = "Text file busy",
    [EFBIG] = "File too large",
    [ENOSPC] = "No space left on device",
    [ESPIPE] = "Illegal seek",
    [EROFS] = "Read-only file system",
    [EMLINK] = "Too many links",
    [EPIPE] = "Broken pipe",
    [EDOM] = "Numerical argument out of domain",
    [ERANGE] = "Numerical result out of range",
    [EDEADLK] = "Resource deadlock avoided",
    [ENAMETOOLONG] = "File name too long",
    [ENOLCK] = "No locks available",
    [ENOSYS] = "Function not implemented",
    [ENOTEMPTY] = "Directory not empty",
    [ELOOP] = "Too many levels of symbolic links",
    [ENOMSG] = "No message of desired type",
    [EIDRM] = "Identifier removed",
    [ENOSTR] = "Device not a stream",
    [ENODATA] = "No data available",
    [ETIME] = "Timer expired",
    [ENOSR] = "Out of streams resources",
    [ENOLINK] = "Link has been severed",
    [EPROTO] = "Protocol error",
    [EMULTIHOP] = "Multihop attempted",
    [EBADMSG] = "Bad message",
    [EOVERFLOW] = "Value too large for defined data type",
    [EILSEQ] = "Invalid or incomplete multibyte or wide character",
    [ENOTSOCK] = "Socket operation on non-socket",
    [EDESTADDRREQ] = "Destination address required",
    [EMSGSIZE] = "Message too long",
    [EPROTOTYPE] = "Protocol wrong type for socket",
    [ENOPROTOOPT] = "Protocol not available",
    [EPROTONOSUPPORT] = "Protocol not supported",
    [ESOCKTNOSUPPORT] = "Socket type not supported",
    [EOPNOTSUPP] = "Operation not supported",
    [EPFNOSUPPORT] = "Protocol family not supported",
    [EAFNOSUPPORT] = "Address family not supported by protocol",
    [EADDRINUSE] = "Address already in use",
    [EADDRNOTAVAIL] = "Cannot assign requested address",
    [ENETDOWN] = "Network is down",
    [ENETUNREACH] = "Network is unreachable",
    [ENETRESET] = "Network dropped connection on reset",
    [ECONNABORTED] = "Software caused connection abort",
    [ECONNRESET] = "Connection reset by peer",
    [ENOBUFS] = "No buffer space available",
    [EISCONN] = "Transport endpoint is already connected",
    [ENOTCONN] = "Transport endpoint is not connected",
    [ETIMEDOUT] = "Connection timed out",
    [ECONNREFUSED] = "Connection refused",
    [EHOSTUNREACH] = "No route to host",
    [EALREADY] = "Operation already in progress",
    [EINPROGRESS] = "Operation now in progress",
    [ESTALE] = "Stale file handle",
    [EDQUOT] = "Disk quota exceeded",
    [ECANCELED] = "Operation canceled",
    [EOWNERDEAD] = "Owner died",
    [ENOTRECOVERABLE] = "State not recoverable",
};

#define UNKNOWN "Unknown error "

static const char *message(int err) {
    if (err >= 0 && (size_t)err < sizeof(messages) / sizeof(*messages)) {
        return messages[err];
    }
    return NULL;
}

int strerror_r(int err, char *buf, size_t n) {
    char tmp[sizeof(UNKNOWN) + 12];
    const char *msg = message(err);
    int ret = 0;
    if (!msg) {
        char *num =
            __fmt_ulong(err < 0 ? -(unsigned long)err : (unsigned long)err,
                        tmp + sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        if (err < 0) {
            *--num = '-';
        }
        num -= sizeof(UNKNOWN) - 1;
        memcpy(num, UNKNOWN, sizeof(UNKNOWN) - 1);
        msg = num;
        ret = EINVAL;
    }
    if (strlcpy(buf, msg, n) >= n) {
        return ERANGE;
    }
    return ret;
}

char *strerror(int err) {
    static __thread char buf[sizeof(UNKNOWN) + 12];
    const char *msg = message(err);
    if (msg) {
        return (char *)msg;
    }
    strerror_r(err, buf, sizeof(buf));
    return buf;
}

char *strerror_l(int err, locale_t loc) {
    (void)loc;
    return strerror(err);
}
