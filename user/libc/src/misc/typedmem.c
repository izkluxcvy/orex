#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

int posix_typed_mem_open(const char *name, int oflag, int tflag) {
    (void)name;
    if ((tflag & ~(POSIX_TYPED_MEM_ALLOCATE | POSIX_TYPED_MEM_ALLOCATE_CONTIG |
                   POSIX_TYPED_MEM_MAP_ALLOCATABLE)) ||
        ((tflag & POSIX_TYPED_MEM_ALLOCATE) &&
         (tflag & POSIX_TYPED_MEM_ALLOCATE_CONTIG)) ||
        (oflag & O_ACCMODE) == 3) {
        errno = EINVAL;
        return -1;
    }
    errno = ENOENT;
    return -1;
}

int posix_typed_mem_get_info(int fildes, struct posix_typed_mem_info *info) {
    (void)info;
    return fcntl(fildes, F_GETFD) < 0 ? EBADF : EINVAL;
}

int posix_mem_offset(const void *__restrict addr, size_t len,
                     off_t *__restrict off, size_t *__restrict contig_len,
                     int *__restrict fildes) {
    (void)addr, (void)len, (void)off, (void)contig_len, (void)fildes;
    return EACCES;
}
