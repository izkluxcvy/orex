#pragma once

#include <stddef.h>

#define SYS_read            0
#define SYS_write           1
#define SYS_open            2
#define SYS_close           3
#define SYS_stat            4
#define SYS_fstat           5
#define SYS_lseek           8
#define SYS_mmap            9
#define SYS_mprotect        10
#define SYS_munmap          11
#define SYS_brk             12
#define SYS_rt_sigaction    13
#define SYS_rt_sigprocmask  14
#define SYS_ioctl           16
#define SYS_access          21
#define SYS_pipe            22
#define SYS_dup             32
#define SYS_dup2            33
#define SYS_getpid          39
#define SYS_fork            57
#define SYS_execve          59
#define SYS_exit            60
#define SYS_wait4           61
#define SYS_kill            62
#define SYS_getcwd          79
#define SYS_chdir           80
#define SYS_mkdir           83
#define SYS_rmdir           84
#define SYS_unlink          87
#define SYS_chmod           90
#define SYS_fchmod          91
#define SYS_chown           92
#define SYS_fchown          93
#define SYS_umask           95
#define SYS_getuid          102
#define SYS_getgid          104
#define SYS_setuid          105
#define SYS_setgid          106
#define SYS_geteuid         107
#define SYS_getegid         108
#define SYS_setpgid         109
#define SYS_getppid         110
#define SYS_setreuid        113
#define SYS_setregid        114
#define SYS_getgroups       115
#define SYS_setgroups       116
#define SYS_setresuid       117
#define SYS_getresuid       118
#define SYS_setresgid       119
#define SYS_getresgid       120
#define SYS_getpgid         121
#define SYS_arch_prctl      158
#define SYS_gettid          186
#define SYS_getdents64      217
#define SYS_set_tid_address 218

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0100
#define O_EXCL   0200
#define O_TRUNC  01000
#define O_APPEND 02000

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED    ((void *)-1)

#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003

#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_ENTRY  9
#define AT_UID    11
#define AT_EUID   12
#define AT_GID    13
#define AT_EGID   14
#define AT_SECURE 23
#define AT_RANDOM 25
#define AT_EXECFN 31

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define SIGINT  2
#define SIGQUIT 3
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGTERM 15
#define SIGCHLD 17

#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIGBIT(sig) (1UL << ((sig) - 1))

typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)

#define TIOCGPGRP 0x540f
#define TIOCSPGRP 0x5410

#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFREG 0100000

#define DT_CHR 2
#define DT_DIR 4
#define DT_REG 8

struct stat {
    unsigned long st_dev, st_ino, st_nlink;
    unsigned int  st_mode, st_uid, st_gid, pad0;
    unsigned long st_rdev;
    long          st_size, st_blksize, st_blocks;
    unsigned long st_atime, st_atime_nsec, st_mtime, st_mtime_nsec;
    unsigned long st_ctime, st_ctime_nsec;
    long          unused[3];
};

struct dirent64 {
    unsigned long  d_ino;
    long           d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
} __attribute__((packed));

#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WIFEXITED(s)   (WTERMSIG(s) == 0)

static inline long syscall4(long nr, long a0, long a1, long a2, long a3) {
    long          ret;
    register long r10 __asm__("r10") = a3;
    __asm__ __volatile__("syscall"
                         : "=a"(ret)
                         : "a"(nr), "D"(a0), "S"(a1), "d"(a2), "r"(r10)
                         : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall6(long nr, long a0, long a1, long a2, long a3,
                            long a4, long a5) {
    long          ret;
    register long r10 __asm__("r10") = a3;
    register long r8 __asm__("r8")   = a4;
    register long r9 __asm__("r9")   = a5;
    __asm__ __volatile__("syscall"
                         : "=a"(ret)
                         : "a"(nr), "D"(a0), "S"(a1), "d"(a2), "r"(r10),
                           "r"(r8), "r"(r9)
                         : "rcx", "r11", "memory");
    return ret;
}

// Raw results: an address, or -errno cast to a pointer.
static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd,
                         long off) {
    return (void *)syscall6(SYS_mmap, (long)addr, (long)len, prot, flags, fd,
                            off);
}
static inline int munmap(void *addr, size_t len) {
    return (int)syscall4(SYS_munmap, (long)addr, (long)len, 0, 0);
}
static inline int mprotect(void *addr, size_t len, int prot) {
    return (int)syscall4(SYS_mprotect, (long)addr, (long)len, prot, 0);
}
static inline void *sys_brk(void *addr) {
    return (void *)syscall4(SYS_brk, (long)addr, 0, 0, 0);
}

static inline long read(int fd, void *buf, size_t n) {
    return syscall4(SYS_read, fd, (long)buf, (long)n, 0);
}
static inline long write(int fd, const void *buf, size_t n) {
    return syscall4(SYS_write, fd, (long)buf, (long)n, 0);
}
static inline int open(const char *path, int flags, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, flags);
    int mode = (flags & O_CREAT) ? __builtin_va_arg(ap, int) : 0;
    __builtin_va_end(ap);
    return (int)syscall4(SYS_open, (long)path, flags, mode, 0);
}
static inline int mkdir(const char *path, int mode) {
    return (int)syscall4(SYS_mkdir, (long)path, mode, 0, 0);
}
static inline int rmdir(const char *path) {
    return (int)syscall4(SYS_rmdir, (long)path, 0, 0, 0);
}
static inline int unlink(const char *path) {
    return (int)syscall4(SYS_unlink, (long)path, 0, 0, 0);
}
static inline int close(int fd) {
    return (int)syscall4(SYS_close, fd, 0, 0, 0);
}
static inline long lseek(int fd, long off, int whence) {
    return syscall4(SYS_lseek, fd, off, whence, 0);
}
static inline int pipe(int fds[2]) {
    return (int)syscall4(SYS_pipe, (long)fds, 0, 0, 0);
}
static inline int dup(int fd) { return (int)syscall4(SYS_dup, fd, 0, 0, 0); }
static inline int dup2(int fd, int nfd) {
    return (int)syscall4(SYS_dup2, fd, nfd, 0, 0);
}
static inline int kill(int pid, int sig) {
    return (int)syscall4(SYS_kill, pid, sig, 0, 0);
}
static inline int sigprocmask(int how, const unsigned long *set,
                              unsigned long *old) {
    return (int)syscall4(SYS_rt_sigprocmask, how, (long)set, (long)old, 8);
}
static inline int setpgid(int pid, int pgid) {
    return (int)syscall4(SYS_setpgid, pid, pgid, 0, 0);
}
static inline int getpgrp(void) {
    return (int)syscall4(SYS_getpgid, 0, 0, 0, 0);
}
static inline int ioctl(int fd, unsigned long req, void *arg) {
    return (int)syscall4(SYS_ioctl, fd, (long)req, (long)arg, 0);
}
static inline int tcsetpgrp(int fd, int pgrp) {
    return ioctl(fd, TIOCSPGRP, &pgrp);
}
static inline int stat(const char *path, struct stat *st) {
    return (int)syscall4(SYS_stat, (long)path, (long)st, 0, 0);
}
static inline int fstat(int fd, struct stat *st) {
    return (int)syscall4(SYS_fstat, fd, (long)st, 0, 0);
}
static inline long getdents64(int fd, void *buf, size_t n) {
    return syscall4(SYS_getdents64, fd, (long)buf, (long)n, 0);
}
static inline int chdir(const char *path) {
    return (int)syscall4(SYS_chdir, (long)path, 0, 0, 0);
}
static inline long getcwd(char *buf, size_t n) {
    return syscall4(SYS_getcwd, (long)buf, (long)n, 0, 0);
}
static inline int getpid(void) { return (int)syscall4(SYS_getpid, 0, 0, 0, 0); }
static inline int getppid(void) {
    return (int)syscall4(SYS_getppid, 0, 0, 0, 0);
}
static inline int fork(void) { return (int)syscall4(SYS_fork, 0, 0, 0, 0); }
static inline int execve(const char *path, char *const argv[],
                         char *const envp[]) {
    return (int)syscall4(SYS_execve, (long)path, (long)argv, (long)envp, 0);
}
static inline int waitpid(int pid, int *status, int options) {
    return (int)syscall4(SYS_wait4, pid, (long)status, options, 0);
}
typedef unsigned int uid_t;
typedef unsigned int gid_t;

static inline int access(const char *path, int mode) {
    return (int)syscall4(SYS_access, (long)path, mode, 0, 0);
}
static inline int chmod(const char *path, int mode) {
    return (int)syscall4(SYS_chmod, (long)path, mode, 0, 0);
}
static inline int fchmod(int fd, int mode) {
    return (int)syscall4(SYS_fchmod, fd, mode, 0, 0);
}
static inline int chown(const char *path, uid_t uid, gid_t gid) {
    return (int)syscall4(SYS_chown, (long)path, (int)uid, (int)gid, 0);
}
static inline int fchown(int fd, uid_t uid, gid_t gid) {
    return (int)syscall4(SYS_fchown, fd, (int)uid, (int)gid, 0);
}
static inline int umask(int mask) {
    return (int)syscall4(SYS_umask, mask, 0, 0, 0);
}
static inline uid_t getuid(void) { return syscall4(SYS_getuid, 0, 0, 0, 0); }
static inline uid_t geteuid(void) { return syscall4(SYS_geteuid, 0, 0, 0, 0); }
static inline gid_t getgid(void) { return syscall4(SYS_getgid, 0, 0, 0, 0); }
static inline gid_t getegid(void) { return syscall4(SYS_getegid, 0, 0, 0, 0); }
static inline int   setuid(uid_t uid) {
    return (int)syscall4(SYS_setuid, (int)uid, 0, 0, 0);
}
static inline int setgid(gid_t gid) {
    return (int)syscall4(SYS_setgid, (int)gid, 0, 0, 0);
}
static inline int setreuid(uid_t r, uid_t e) {
    return (int)syscall4(SYS_setreuid, (int)r, (int)e, 0, 0);
}
static inline int setregid(gid_t r, gid_t e) {
    return (int)syscall4(SYS_setregid, (int)r, (int)e, 0, 0);
}
static inline int setresuid(uid_t r, uid_t e, uid_t s) {
    return (int)syscall4(SYS_setresuid, (int)r, (int)e, (int)s, 0);
}
static inline int setresgid(gid_t r, gid_t e, gid_t s) {
    return (int)syscall4(SYS_setresgid, (int)r, (int)e, (int)s, 0);
}
static inline int getresuid(uid_t *r, uid_t *e, uid_t *s) {
    return (int)syscall4(SYS_getresuid, (long)r, (long)e, (long)s, 0);
}
static inline int getresgid(gid_t *r, gid_t *e, gid_t *s) {
    return (int)syscall4(SYS_getresgid, (long)r, (long)e, (long)s, 0);
}
static inline int getgroups(int n, gid_t *list) {
    return (int)syscall4(SYS_getgroups, n, (long)list, 0, 0);
}
static inline int setgroups(size_t n, const gid_t *list) {
    return (int)syscall4(SYS_setgroups, (long)n, (long)list, 0, 0);
}
static inline int arch_prctl(int code, unsigned long addr) {
    return (int)syscall4(SYS_arch_prctl, code, (long)addr, 0, 0);
}
static inline int set_tid_address(int *tidptr) {
    return (int)syscall4(SYS_set_tid_address, (long)tidptr, 0, 0, 0);
}
static inline int gettid(void) { return (int)syscall4(SYS_gettid, 0, 0, 0, 0); }
[[noreturn]] static inline void _exit(int status) {
    syscall4(SYS_exit, status, 0, 0, 0);
    __builtin_unreachable();
}

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
char  *strchr(const char *s, int c);
long   strtol(const char *s, char **end, int base);
void  *memset(void *d, int c, size_t n);
void  *memcpy(void *d, const void *s, size_t n);

sighandler_t signal(int sig, sighandler_t handler);

extern char **environ;
unsigned long getauxval(unsigned long type);

void puts(const char *s); // no trailing newline, unlike stdio
void putnum(long v);
