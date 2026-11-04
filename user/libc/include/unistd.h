#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_pid_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_off_t
#define __NEED_intptr_t
#define __NEED_useconds_t
#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>

#define _POSIX_VERSION                    200809L
#define _POSIX_CHOWN_RESTRICTED           1
#define _POSIX_NO_TRUNC                   1
#define _POSIX_SAVED_IDS                  1
#define _POSIX_JOB_CONTROL                1
#define _POSIX_VDISABLE                   '\0'
#define _POSIX_THREADS                    200809L
#define _POSIX_THREAD_SAFE_FUNCTIONS      200809L
#define _POSIX_THREAD_ATTR_STACKADDR      200809L
#define _POSIX_THREAD_ATTR_STACKSIZE      200809L
#define _POSIX_THREAD_CPUTIME             200809L
#define _POSIX_BARRIERS                   200809L
#define _POSIX_SPIN_LOCKS                 200809L
#define _POSIX_READER_WRITER_LOCKS        200809L
#define _POSIX_TIMEOUTS                   200809L
#define _POSIX_CLOCK_SELECTION            200809L
#define _POSIX_THREAD_PROCESS_SHARED      200809L
#define _POSIX_SHARED_MEMORY_OBJECTS      200809L
#define _POSIX_MAPPED_FILES               200809L
#define _POSIX_THREAD_PRIO_INHERIT        (-1)
#define _POSIX_THREAD_PRIO_PROTECT        (-1)
#define _POSIX_THREAD_PRIORITY_SCHEDULING 200809L
#define _POSIX_PRIORITY_SCHEDULING        200809L
#define _POSIX_MEMLOCK                    200809L
#define _POSIX_ASYNCHRONOUS_IO            200809L
#define _POSIX_MESSAGE_PASSING            200809L
#define _POSIX_MEMLOCK_RANGE              200809L
#define _POSIX_SEMAPHORES                 200809L
#define _POSIX_TYPED_MEMORY_OBJECTS       (-1)
#define _POSIX_MEMORY_PROTECTION          200809L
#define _POSIX_REALTIME_SIGNALS           200809L
#define _POSIX_TIMERS                     200809L
#define _POSIX_MONOTONIC_CLOCK            200809L
#define _POSIX_CPUTIME                    200809L
#define _POSIX_SPAWN                      200809L
#define _POSIX_IPV6                       200809L
#define _POSIX_FSYNC                      200809L
#define _POSIX_ADVISORY_INFO              200809L
#define _POSIX_REGEXP                     1
#define _POSIX_SHELL                      1
#define _POSIX_RAW_SOCKETS                (-1)
#define _POSIX_PRIORITIZED_IO             (-1)
#define _POSIX_SPORADIC_SERVER            (-1)
#define _POSIX_THREAD_SPORADIC_SERVER     (-1)
#define _POSIX_THREAD_ROBUST_PRIO_INHERIT (-1)
#define _POSIX_THREAD_ROBUST_PRIO_PROTECT (-1)

#define _POSIX2_VERSION   200809L
#define _POSIX2_C_BIND    200809L
#define _POSIX2_C_DEV     (-1) // the utilities, not yet all there
#define _POSIX2_CHAR_TERM (-1)
#define _POSIX2_FORT_RUN  (-1)
#define _POSIX2_LOCALEDEF (-1)
#define _POSIX2_SW_DEV    (-1)
#define _POSIX2_UPE       (-1)

#define _XOPEN_VERSION          700
#define _XOPEN_CRYPT            1
#define _XOPEN_ENH_I18N         1
#define _XOPEN_SHM              1
#define _XOPEN_UNIX             (-1)
#define _XOPEN_REALTIME         (-1)
#define _XOPEN_REALTIME_THREADS (-1)
#define _XOPEN_UUCP             (-1)

// The one environment: LP64 with a 64-bit off_t.
#define _POSIX_V8_LP64_OFF64   1
#define _POSIX_V8_ILP32_OFF32  (-1)
#define _POSIX_V8_ILP32_OFFBIG (-1)
#define _POSIX_V8_LPBIG_OFFBIG (-1)
#define _POSIX_V7_LP64_OFF64   1
#define _POSIX_V7_ILP32_OFF32  (-1)
#define _POSIX_V7_ILP32_OFFBIG (-1)
#define _POSIX_V7_LPBIG_OFFBIG (-1)

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define POSIX_CLOSE_RESTART 0

#define F_ULOCK 0 // lockf's
#define F_LOCK  1
#define F_TLOCK 2
#define F_TEST  3

enum {
    _SC_ARG_MAX,
    _SC_CHILD_MAX,
    _SC_CLK_TCK,
    _SC_NGROUPS_MAX,
    _SC_OPEN_MAX,
    _SC_STREAM_MAX,
    _SC_TZNAME_MAX,
    _SC_JOB_CONTROL,
    _SC_SAVED_IDS,
    _SC_VERSION,
    _SC_PAGESIZE,
    _SC_PAGE_SIZE = _SC_PAGESIZE,
    _SC_LINE_MAX,
    _SC_IOV_MAX,
    _SC_HOST_NAME_MAX,
    _SC_LOGIN_NAME_MAX,
    _SC_TTY_NAME_MAX,
    _SC_SYMLOOP_MAX,
    _SC_RE_DUP_MAX,
    _SC_BC_BASE_MAX,
    _SC_BC_DIM_MAX,
    _SC_BC_SCALE_MAX,
    _SC_BC_STRING_MAX,
    _SC_COLL_WEIGHTS_MAX,
    _SC_EXPR_NEST_MAX,
    _SC_GETGR_R_SIZE_MAX,
    _SC_GETPW_R_SIZE_MAX,
    _SC_NPROCESSORS_CONF,
    _SC_NPROCESSORS_ONLN,
    _SC_THREADS,
    _SC_THREAD_SAFE_FUNCTIONS,
    _SC_THREAD_KEYS_MAX,
    _SC_THREAD_STACK_MIN,
    _SC_THREAD_THREADS_MAX,
    _SC_THREAD_DESTRUCTOR_ITERATIONS,
    _SC_SEM_VALUE_MAX,
    _SC_2_VERSION,
    _SC_ASYNCHRONOUS_IO,
    _SC_AIO_LISTIO_MAX,
    _SC_AIO_MAX,
    _SC_AIO_PRIO_DELTA_MAX,
    _SC_MESSAGE_PASSING,
    _SC_MQ_OPEN_MAX,
    _SC_MQ_PRIO_MAX,
    _SC_SEM_NSEMS_MAX,
    _SC_XOPEN_CRYPT,
    _SC_TYPED_MEMORY_OBJECTS,
    _SC_2_C_BIND,
    _SC_2_C_DEV,
    _SC_2_CHAR_TERM,
    _SC_2_FORT_RUN,
    _SC_2_LOCALEDEF,
    _SC_2_SW_DEV,
    _SC_2_UPE,
    _SC_ADVISORY_INFO,
    _SC_ATEXIT_MAX,
    _SC_BARRIERS,
    _SC_CLOCK_SELECTION,
    _SC_CPUTIME,
    _SC_DELAYTIMER_MAX,
    _SC_FSYNC,
    _SC_IPV6,
    _SC_MAPPED_FILES,
    _SC_MEMLOCK,
    _SC_MEMLOCK_RANGE,
    _SC_MEMORY_PROTECTION,
    _SC_MONOTONIC_CLOCK,
    _SC_NSIG,
    _SC_PRIORITIZED_IO,
    _SC_PRIORITY_SCHEDULING,
    _SC_RAW_SOCKETS,
    _SC_READER_WRITER_LOCKS,
    _SC_REALTIME_SIGNALS,
    _SC_REGEXP,
    _SC_RTSIG_MAX,
    _SC_SEMAPHORES,
    _SC_SHARED_MEMORY_OBJECTS,
    _SC_SHELL,
    _SC_SIGQUEUE_MAX,
    _SC_SPAWN,
    _SC_SPIN_LOCKS,
    _SC_SPORADIC_SERVER,
    _SC_SS_REPL_MAX,
    _SC_SYNCHRONIZED_IO,
    _SC_THREAD_ATTR_STACKADDR,
    _SC_THREAD_ATTR_STACKSIZE,
    _SC_THREAD_CPUTIME,
    _SC_THREAD_PRIO_INHERIT,
    _SC_THREAD_PRIO_PROTECT,
    _SC_THREAD_PRIORITY_SCHEDULING,
    _SC_THREAD_PROCESS_SHARED,
    _SC_THREAD_ROBUST_PRIO_INHERIT,
    _SC_THREAD_ROBUST_PRIO_PROTECT,
    _SC_THREAD_SPORADIC_SERVER,
    _SC_TIMEOUTS,
    _SC_TIMER_MAX,
    _SC_TIMERS,
    _SC_V8_ILP32_OFF32,
    _SC_V8_ILP32_OFFBIG,
    _SC_V8_LP64_OFF64,
    _SC_V8_LPBIG_OFFBIG,
    _SC_V7_ILP32_OFF32,
    _SC_V7_ILP32_OFFBIG,
    _SC_V7_LP64_OFF64,
    _SC_V7_LPBIG_OFFBIG,
    _SC_XOPEN_ENH_I18N,
    _SC_XOPEN_REALTIME,
    _SC_XOPEN_REALTIME_THREADS,
    _SC_XOPEN_SHM,
    _SC_XOPEN_UNIX,
    _SC_XOPEN_UUCP,
    _SC_XOPEN_VERSION,
};
// Each a macro as well, for #ifdef.
#define _SC_ARG_MAX                      _SC_ARG_MAX
#define _SC_CHILD_MAX                    _SC_CHILD_MAX
#define _SC_CLK_TCK                      _SC_CLK_TCK
#define _SC_NGROUPS_MAX                  _SC_NGROUPS_MAX
#define _SC_OPEN_MAX                     _SC_OPEN_MAX
#define _SC_STREAM_MAX                   _SC_STREAM_MAX
#define _SC_TZNAME_MAX                   _SC_TZNAME_MAX
#define _SC_JOB_CONTROL                  _SC_JOB_CONTROL
#define _SC_SAVED_IDS                    _SC_SAVED_IDS
#define _SC_VERSION                      _SC_VERSION
#define _SC_PAGESIZE                     _SC_PAGESIZE
#define _SC_PAGE_SIZE                    _SC_PAGE_SIZE
#define _SC_LINE_MAX                     _SC_LINE_MAX
#define _SC_IOV_MAX                      _SC_IOV_MAX
#define _SC_HOST_NAME_MAX                _SC_HOST_NAME_MAX
#define _SC_LOGIN_NAME_MAX               _SC_LOGIN_NAME_MAX
#define _SC_TTY_NAME_MAX                 _SC_TTY_NAME_MAX
#define _SC_SYMLOOP_MAX                  _SC_SYMLOOP_MAX
#define _SC_RE_DUP_MAX                   _SC_RE_DUP_MAX
#define _SC_BC_BASE_MAX                  _SC_BC_BASE_MAX
#define _SC_BC_DIM_MAX                   _SC_BC_DIM_MAX
#define _SC_BC_SCALE_MAX                 _SC_BC_SCALE_MAX
#define _SC_BC_STRING_MAX                _SC_BC_STRING_MAX
#define _SC_COLL_WEIGHTS_MAX             _SC_COLL_WEIGHTS_MAX
#define _SC_EXPR_NEST_MAX                _SC_EXPR_NEST_MAX
#define _SC_GETGR_R_SIZE_MAX             _SC_GETGR_R_SIZE_MAX
#define _SC_GETPW_R_SIZE_MAX             _SC_GETPW_R_SIZE_MAX
#define _SC_NPROCESSORS_CONF             _SC_NPROCESSORS_CONF
#define _SC_NPROCESSORS_ONLN             _SC_NPROCESSORS_ONLN
#define _SC_THREADS                      _SC_THREADS
#define _SC_THREAD_SAFE_FUNCTIONS        _SC_THREAD_SAFE_FUNCTIONS
#define _SC_THREAD_KEYS_MAX              _SC_THREAD_KEYS_MAX
#define _SC_THREAD_STACK_MIN             _SC_THREAD_STACK_MIN
#define _SC_THREAD_THREADS_MAX           _SC_THREAD_THREADS_MAX
#define _SC_THREAD_DESTRUCTOR_ITERATIONS _SC_THREAD_DESTRUCTOR_ITERATIONS
#define _SC_SEM_VALUE_MAX                _SC_SEM_VALUE_MAX
#define _SC_2_VERSION                    _SC_2_VERSION
#define _SC_ASYNCHRONOUS_IO              _SC_ASYNCHRONOUS_IO
#define _SC_AIO_LISTIO_MAX               _SC_AIO_LISTIO_MAX
#define _SC_AIO_MAX                      _SC_AIO_MAX
#define _SC_AIO_PRIO_DELTA_MAX           _SC_AIO_PRIO_DELTA_MAX
#define _SC_MESSAGE_PASSING              _SC_MESSAGE_PASSING
#define _SC_MQ_OPEN_MAX                  _SC_MQ_OPEN_MAX
#define _SC_MQ_PRIO_MAX                  _SC_MQ_PRIO_MAX
#define _SC_SEM_NSEMS_MAX                _SC_SEM_NSEMS_MAX
#define _SC_XOPEN_CRYPT                  _SC_XOPEN_CRYPT
#define _SC_TYPED_MEMORY_OBJECTS         _SC_TYPED_MEMORY_OBJECTS
#define _SC_2_C_BIND                     _SC_2_C_BIND
#define _SC_2_C_DEV                      _SC_2_C_DEV
#define _SC_2_CHAR_TERM                  _SC_2_CHAR_TERM
#define _SC_2_FORT_RUN                   _SC_2_FORT_RUN
#define _SC_2_LOCALEDEF                  _SC_2_LOCALEDEF
#define _SC_2_SW_DEV                     _SC_2_SW_DEV
#define _SC_2_UPE                        _SC_2_UPE
#define _SC_ADVISORY_INFO                _SC_ADVISORY_INFO
#define _SC_ATEXIT_MAX                   _SC_ATEXIT_MAX
#define _SC_BARRIERS                     _SC_BARRIERS
#define _SC_CLOCK_SELECTION              _SC_CLOCK_SELECTION
#define _SC_CPUTIME                      _SC_CPUTIME
#define _SC_DELAYTIMER_MAX               _SC_DELAYTIMER_MAX
#define _SC_FSYNC                        _SC_FSYNC
#define _SC_IPV6                         _SC_IPV6
#define _SC_MAPPED_FILES                 _SC_MAPPED_FILES
#define _SC_MEMLOCK                      _SC_MEMLOCK
#define _SC_MEMLOCK_RANGE                _SC_MEMLOCK_RANGE
#define _SC_MEMORY_PROTECTION            _SC_MEMORY_PROTECTION
#define _SC_MONOTONIC_CLOCK              _SC_MONOTONIC_CLOCK
#define _SC_NSIG                         _SC_NSIG
#define _SC_PRIORITIZED_IO               _SC_PRIORITIZED_IO
#define _SC_PRIORITY_SCHEDULING          _SC_PRIORITY_SCHEDULING
#define _SC_RAW_SOCKETS                  _SC_RAW_SOCKETS
#define _SC_READER_WRITER_LOCKS          _SC_READER_WRITER_LOCKS
#define _SC_REALTIME_SIGNALS             _SC_REALTIME_SIGNALS
#define _SC_REGEXP                       _SC_REGEXP
#define _SC_RTSIG_MAX                    _SC_RTSIG_MAX
#define _SC_SEMAPHORES                   _SC_SEMAPHORES
#define _SC_SHARED_MEMORY_OBJECTS        _SC_SHARED_MEMORY_OBJECTS
#define _SC_SHELL                        _SC_SHELL
#define _SC_SIGQUEUE_MAX                 _SC_SIGQUEUE_MAX
#define _SC_SPAWN                        _SC_SPAWN
#define _SC_SPIN_LOCKS                   _SC_SPIN_LOCKS
#define _SC_SPORADIC_SERVER              _SC_SPORADIC_SERVER
#define _SC_SS_REPL_MAX                  _SC_SS_REPL_MAX
#define _SC_SYNCHRONIZED_IO              _SC_SYNCHRONIZED_IO
#define _SC_THREAD_ATTR_STACKADDR        _SC_THREAD_ATTR_STACKADDR
#define _SC_THREAD_ATTR_STACKSIZE        _SC_THREAD_ATTR_STACKSIZE
#define _SC_THREAD_CPUTIME               _SC_THREAD_CPUTIME
#define _SC_THREAD_PRIO_INHERIT          _SC_THREAD_PRIO_INHERIT
#define _SC_THREAD_PRIO_PROTECT          _SC_THREAD_PRIO_PROTECT
#define _SC_THREAD_PRIORITY_SCHEDULING   _SC_THREAD_PRIORITY_SCHEDULING
#define _SC_THREAD_PROCESS_SHARED        _SC_THREAD_PROCESS_SHARED
#define _SC_THREAD_ROBUST_PRIO_INHERIT   _SC_THREAD_ROBUST_PRIO_INHERIT
#define _SC_THREAD_ROBUST_PRIO_PROTECT   _SC_THREAD_ROBUST_PRIO_PROTECT
#define _SC_THREAD_SPORADIC_SERVER       _SC_THREAD_SPORADIC_SERVER
#define _SC_TIMEOUTS                     _SC_TIMEOUTS
#define _SC_TIMER_MAX                    _SC_TIMER_MAX
#define _SC_TIMERS                       _SC_TIMERS
#define _SC_V8_ILP32_OFF32               _SC_V8_ILP32_OFF32
#define _SC_V8_ILP32_OFFBIG              _SC_V8_ILP32_OFFBIG
#define _SC_V8_LP64_OFF64                _SC_V8_LP64_OFF64
#define _SC_V8_LPBIG_OFFBIG              _SC_V8_LPBIG_OFFBIG
#define _SC_V7_ILP32_OFF32               _SC_V7_ILP32_OFF32
#define _SC_V7_ILP32_OFFBIG              _SC_V7_ILP32_OFFBIG
#define _SC_V7_LP64_OFF64                _SC_V7_LP64_OFF64
#define _SC_V7_LPBIG_OFFBIG              _SC_V7_LPBIG_OFFBIG
#define _SC_XOPEN_ENH_I18N               _SC_XOPEN_ENH_I18N
#define _SC_XOPEN_REALTIME               _SC_XOPEN_REALTIME
#define _SC_XOPEN_REALTIME_THREADS       _SC_XOPEN_REALTIME_THREADS
#define _SC_XOPEN_SHM                    _SC_XOPEN_SHM
#define _SC_XOPEN_UNIX                   _SC_XOPEN_UNIX
#define _SC_XOPEN_UUCP                   _SC_XOPEN_UUCP
#define _SC_XOPEN_VERSION                _SC_XOPEN_VERSION

enum {
    _PC_LINK_MAX,
    _PC_MAX_CANON,
    _PC_MAX_INPUT,
    _PC_NAME_MAX,
    _PC_PATH_MAX,
    _PC_PIPE_BUF,
    _PC_CHOWN_RESTRICTED,
    _PC_NO_TRUNC,
    _PC_VDISABLE,
    _PC_FILESIZEBITS,
    _PC_SYMLINK_MAX,
    _PC_2_SYMLINKS,
    _PC_TIMESTAMP_RESOLUTION,
    _PC_ALLOC_SIZE_MIN,
    _PC_ASYNC_IO,
    _PC_PRIO_IO,
    _PC_SYNC_IO,
    _PC_REC_INCR_XFER_SIZE,
    _PC_REC_MAX_XFER_SIZE,
    _PC_REC_MIN_XFER_SIZE,
    _PC_REC_XFER_ALIGN,
    _PC_TEXTDOMAIN_MAX,
};
// Each a macro as well, for #ifdef.
#define _PC_LINK_MAX             _PC_LINK_MAX
#define _PC_MAX_CANON            _PC_MAX_CANON
#define _PC_MAX_INPUT            _PC_MAX_INPUT
#define _PC_NAME_MAX             _PC_NAME_MAX
#define _PC_PATH_MAX             _PC_PATH_MAX
#define _PC_PIPE_BUF             _PC_PIPE_BUF
#define _PC_CHOWN_RESTRICTED     _PC_CHOWN_RESTRICTED
#define _PC_NO_TRUNC             _PC_NO_TRUNC
#define _PC_VDISABLE             _PC_VDISABLE
#define _PC_FILESIZEBITS         _PC_FILESIZEBITS
#define _PC_SYMLINK_MAX          _PC_SYMLINK_MAX
#define _PC_2_SYMLINKS           _PC_2_SYMLINKS
#define _PC_TIMESTAMP_RESOLUTION _PC_TIMESTAMP_RESOLUTION
#define _PC_ALLOC_SIZE_MIN       _PC_ALLOC_SIZE_MIN
#define _PC_ASYNC_IO             _PC_ASYNC_IO
#define _PC_PRIO_IO              _PC_PRIO_IO
#define _PC_SYNC_IO              _PC_SYNC_IO
#define _PC_REC_INCR_XFER_SIZE   _PC_REC_INCR_XFER_SIZE
#define _PC_REC_MAX_XFER_SIZE    _PC_REC_MAX_XFER_SIZE
#define _PC_REC_MIN_XFER_SIZE    _PC_REC_MIN_XFER_SIZE
#define _PC_REC_XFER_ALIGN       _PC_REC_XFER_ALIGN
#define _PC_TEXTDOMAIN_MAX       _PC_TEXTDOMAIN_MAX

enum {
    _CS_PATH,
    _CS_V8_ENV,
    _CS_V7_ENV,
    _CS_POSIX_V8_LP64_OFF64_CFLAGS,
    _CS_POSIX_V8_LP64_OFF64_LDFLAGS,
    _CS_POSIX_V8_LP64_OFF64_LIBS,
    _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS,
    _CS_POSIX_V7_LP64_OFF64_CFLAGS,
    _CS_POSIX_V7_LP64_OFF64_LDFLAGS,
    _CS_POSIX_V7_LP64_OFF64_LIBS,
    _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS,
};
// Each a macro as well, for #ifdef.
#define _CS_PATH                           _CS_PATH
#define _CS_V8_ENV                         _CS_V8_ENV
#define _CS_V7_ENV                         _CS_V7_ENV
#define _CS_POSIX_V8_LP64_OFF64_CFLAGS     _CS_POSIX_V8_LP64_OFF64_CFLAGS
#define _CS_POSIX_V8_LP64_OFF64_LDFLAGS    _CS_POSIX_V8_LP64_OFF64_LDFLAGS
#define _CS_POSIX_V8_LP64_OFF64_LIBS       _CS_POSIX_V8_LP64_OFF64_LIBS
#define _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS _CS_POSIX_V8_WIDTH_RESTRICTED_ENVS
#define _CS_POSIX_V7_LP64_OFF64_CFLAGS     _CS_POSIX_V7_LP64_OFF64_CFLAGS
#define _CS_POSIX_V7_LP64_OFF64_LDFLAGS    _CS_POSIX_V7_LP64_OFF64_LDFLAGS
#define _CS_POSIX_V7_LP64_OFF64_LIBS       _CS_POSIX_V7_LP64_OFF64_LIBS
#define _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS _CS_POSIX_V7_WIDTH_RESTRICTED_ENVS

int access(const char *path, int mode);
unsigned alarm(unsigned seconds);
int chdir(const char *path);
int chroot(const char *path); // legacy
int chown(const char *path, uid_t uid, gid_t gid);
int close(int fd);
size_t confstr(int name, char *buf, size_t len);
char *crypt(const char *key, const char *salt);
int dup(int fd);
int dup2(int fd, int nfd);
int dup3(int fd, int nfd, int flags);
void encrypt(char block[64], int edflag);
int execl(const char *path, const char *arg, ...);
int execle(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execv(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
int faccessat(int dirfd, const char *path, int mode, int flags);
int fchdir(int fd);
int fchown(int fd, uid_t uid, gid_t gid);
int fchownat(int dirfd, const char *path, uid_t uid, gid_t gid, int flags);
int fdatasync(int fd);
int fexecve(int fd, char *const argv[], char *const envp[]);
pid_t fork(void);
pid_t _Fork(void);
long fpathconf(int fd, int name);
int fsync(int fd);
int ftruncate(int fd, off_t len);
char *getcwd(char *buf, size_t size);
long gethostid(void);
int gethostname(char *name, size_t len);
gid_t getegid(void);
uid_t geteuid(void);
gid_t getgid(void);
int getgroups(int n, gid_t list[]);
int getentropy(void *buf, size_t n);
char *getpass(const char *prompt); // legacy, but login and su still want it
pid_t getsid(pid_t pid);
char *getlogin(void);
int getlogin_r(char *buf, size_t size);
int isatty(int fd);
pid_t getpgid(pid_t pid);
pid_t getpgrp(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
int lchown(const char *path, uid_t uid, gid_t gid);
int link(const char *from, const char *to);
int linkat(int fromdirfd, const char *from, int todirfd, const char *to,
           int flags);
int lockf(int fd, int cmd, off_t len);
off_t lseek(int fd, off_t off, int whence);
long pathconf(const char *path, int name);
int nice(int incr);
int pause(void);
int pipe(int fds[2]);
int posix_close(int fd, int flag);
int pipe2(int fds[2], int flags);
ssize_t pread(int fd, void *buf, size_t n, off_t off);
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off);
ssize_t read(int fd, void *buf, size_t n);
ssize_t readlink(const char *__restrict path, char *__restrict buf,
                 size_t size);
ssize_t readlinkat(int dirfd, const char *__restrict path, char *__restrict buf,
                   size_t size);
int rmdir(const char *path);
int setegid(gid_t gid);
int seteuid(uid_t uid);
int setgid(gid_t gid);
int setpgid(pid_t pid, pid_t pgid);
int setregid(gid_t rgid, gid_t egid);
int setreuid(uid_t ruid, uid_t euid);
int setuid(uid_t uid);
pid_t setsid(void);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int truncate(const char *path, off_t len);
char *ttyname(int fd);
int ttyname_r(int fd, char *buf, size_t size);
unsigned sleep(unsigned seconds);
int usleep(unsigned usec); // gone from POSIX, still called
void swab(const void *__restrict src, void *__restrict dst, ssize_t n);
int symlink(const char *target, const char *path);
int symlinkat(const char *target, int dirfd, const char *path);
void sync(void);
long sysconf(int name);
int unlink(const char *path);
int unlinkat(int dirfd, const char *path, int flags);
ssize_t write(int fd, const void *buf, size_t n);

__attribute__((__noreturn__)) void _exit(int status);

// Not POSIX, but as in glibc and musl.
int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int setgroups(size_t n, const gid_t *list);
int sethostname(const char *name, size_t len);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
long syscall(long number, ...);

extern char **environ;

extern char *optarg;
extern int opterr, optind, optopt;
int getopt(int argc, char *const argv[], const char *opts);

#ifdef __cplusplus
}
#endif
