#pragma once

#include <stddef.h>
#include <stdint.h>

struct file;
struct proc;

#define FD_CLOEXEC 1
#define FD_CLOFORK 2

static inline int fd_flags_of(long oflags) {
    return (oflags & 02000000 ? FD_CLOEXEC : 0) |
           (oflags & 040000000 ? FD_CLOFORK : 0);
}

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

#define F_OWNER_PID  1
#define F_OWNER_PGRP 2

struct file *fd_get(long fd);
struct file *fd_get_io(long fd);
void         fd_return_borrowed();

int fd_install(struct file *f, long min, int fdflags);

void fd_close_all(struct proc *p);
void fd_close_on_exec(struct proc *p);
void fd_fork(struct proc *child, struct proc *parent);

long sys_read(long fd, void *buf, long n);
long sys_write(long fd, const void *buf, long n);
long sys_lseek(long fd, long offset, long whence);
long sys_close(long fd);
long sys_dup(long fd);
long sys_dup3(long fd, long nfd, long flags, int allow_same);
long sys_fcntl(long fd, long cmd, long arg);
long sys_pipe2(int *ufds, long flags);
