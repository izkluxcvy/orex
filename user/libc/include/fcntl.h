#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_mode_t
#define __NEED_off_t
#define __NEED_pid_t
#include <bits/alltypes.h>

#include <bits/fmode.h>

#define O_RDONLY    0
#define O_WRONLY    01
#define O_RDWR      02
#define O_ACCMODE   03
#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_NDELAY    O_NONBLOCK
#define O_DSYNC     010000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC   02000000
#define O_SYNC      04010000
#define O_RSYNC     O_SYNC
#define O_PATH      010000000
#define O_SEARCH    O_PATH
#define O_EXEC      O_PATH
#define O_CLOFORK   040000000
#define O_TTY_INIT  0

#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_GETLK         5
#define F_SETLK         6
#define F_SETLKW        7
#define F_SETOWN        8
#define F_GETOWN        9
#define F_SETOWN_EX     15
#define F_GETOWN_EX     16
#define F_OFD_GETLK     36
#define F_OFD_SETLK     37
#define F_OFD_SETLKW    38
#define F_DUPFD_CLOEXEC 1030
#define F_DUPFD_CLOFORK 1100

#define FD_CLOEXEC 1
#define FD_CLOFORK 2

#define F_OWNER_PID  1
#define F_OWNER_PGRP 2

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_EACCESS          0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EMPTY_PATH       0x1000

#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_RANDOM     1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED   3
#define POSIX_FADV_DONTNEED   4
#define POSIX_FADV_NOREUSE    5

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

struct f_owner_ex {
    int type;
    pid_t pid;
};

int creat(const char *path, mode_t mode);
int fcntl(int fd, int cmd, ...);
int open(const char *path, int flags, ...);
int openat(int dirfd, const char *path, int flags, ...);
int posix_fadvise(int fd, off_t off, off_t len, int advice);
int posix_fallocate(int fd, off_t off, off_t len);

#ifdef __cplusplus
}
#endif
