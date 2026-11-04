#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_off_t
#define __NEED_mode_t
#include <bits/alltypes.h>

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_FAILED    ((void *)-1)

#define MS_ASYNC      1
#define MS_INVALIDATE 2
#define MS_SYNC       4

#define POSIX_MADV_NORMAL     0
#define POSIX_MADV_RANDOM     1
#define POSIX_MADV_SEQUENTIAL 2
#define POSIX_MADV_WILLNEED   3
#define POSIX_MADV_DONTNEED   4
#define MADV_NORMAL           0
#define MADV_RANDOM           1
#define MADV_SEQUENTIAL       2
#define MADV_WILLNEED         3
#define MADV_DONTNEED         4

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int mprotect(void *addr, size_t len, int prot);
#define MCL_CURRENT 1
#define MCL_FUTURE  2

int mlock(const void *addr, size_t len);
int mlockall(int flags);
int msync(void *addr, size_t len, int flags);
int munlock(const void *addr, size_t len);
int munlockall(void);
int munmap(void *addr, size_t len);
int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);
int posix_madvise(void *addr, size_t len, int advice);
int madvise(void *addr, size_t len, int advice);

#define POSIX_TYPED_MEM_ALLOCATE        1
#define POSIX_TYPED_MEM_ALLOCATE_CONTIG 2
#define POSIX_TYPED_MEM_MAP_ALLOCATABLE 4

struct posix_typed_mem_info {
    size_t posix_tmi_length;
};

int posix_mem_offset(const void *__restrict addr, size_t len,
                     off_t *__restrict off, size_t *__restrict contig_len,
                     int *__restrict fildes);
int posix_typed_mem_get_info(int fildes, struct posix_typed_mem_info *info);
int posix_typed_mem_open(const char *name, int oflag, int tflag);

#ifdef __cplusplus
}
#endif
