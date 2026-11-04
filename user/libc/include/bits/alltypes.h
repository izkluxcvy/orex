#if defined(__NEED_size_t) && !defined(__DEFINED_size_t)
typedef __SIZE_TYPE__ size_t;
#define __DEFINED_size_t
#endif

#if defined(__NEED_ssize_t) && !defined(__DEFINED_ssize_t)
typedef long ssize_t;
#define __DEFINED_ssize_t
#endif

#if defined(__NEED_intptr_t) && !defined(__DEFINED_intptr_t)
typedef __INTPTR_TYPE__ intptr_t;
#define __DEFINED_intptr_t
#endif

#if defined(__NEED_wchar_t) && !defined(__DEFINED_wchar_t) &&                  \
    !defined(__cplusplus) // C++'s own
typedef __WCHAR_TYPE__ wchar_t;
#define __DEFINED_wchar_t
#endif

#if defined(__NEED_pid_t) && !defined(__DEFINED_pid_t)
typedef int pid_t;
#define __DEFINED_pid_t
#endif

#if defined(__NEED_uid_t) && !defined(__DEFINED_uid_t)
typedef unsigned int uid_t;
#define __DEFINED_uid_t
#endif

#if defined(__NEED_gid_t) && !defined(__DEFINED_gid_t)
typedef unsigned int gid_t;
#define __DEFINED_gid_t
#endif

#if defined(__NEED_id_t) && !defined(__DEFINED_id_t)
typedef unsigned int id_t;
#define __DEFINED_id_t
#endif

#if defined(__NEED_off_t) && !defined(__DEFINED_off_t)
typedef long off_t;
#define __DEFINED_off_t
#endif

#if defined(__NEED_mode_t) && !defined(__DEFINED_mode_t)
typedef unsigned int mode_t;
#define __DEFINED_mode_t
#endif

#if defined(__NEED_dev_t) && !defined(__DEFINED_dev_t)
typedef unsigned long dev_t;
#define __DEFINED_dev_t
#endif

#if defined(__NEED_ino_t) && !defined(__DEFINED_ino_t)
typedef unsigned long ino_t;
#define __DEFINED_ino_t
#endif

#if defined(__NEED_nlink_t) && !defined(__DEFINED_nlink_t)
typedef unsigned long nlink_t;
#define __DEFINED_nlink_t
#endif

#if defined(__NEED_blksize_t) && !defined(__DEFINED_blksize_t)
typedef long blksize_t;
#define __DEFINED_blksize_t
#endif

#if defined(__NEED_blkcnt_t) && !defined(__DEFINED_blkcnt_t)
typedef long blkcnt_t;
#define __DEFINED_blkcnt_t
#endif

#if defined(__NEED_fsblkcnt_t) && !defined(__DEFINED_fsblkcnt_t)
typedef unsigned long fsblkcnt_t;
#define __DEFINED_fsblkcnt_t
#endif

#if defined(__NEED_fsfilcnt_t) && !defined(__DEFINED_fsfilcnt_t)
typedef unsigned long fsfilcnt_t;
#define __DEFINED_fsfilcnt_t
#endif

#if defined(__NEED_time_t) && !defined(__DEFINED_time_t)
typedef long time_t;
#define __DEFINED_time_t
#endif

#if defined(__NEED_clock_t) && !defined(__DEFINED_clock_t)
typedef long clock_t;
#define __DEFINED_clock_t
#endif

#if defined(__NEED_clockid_t) && !defined(__DEFINED_clockid_t)
typedef int clockid_t;
#define __DEFINED_clockid_t
#endif

#if defined(__NEED_timer_t) && !defined(__DEFINED_timer_t)
typedef void *timer_t;
#define __DEFINED_timer_t
#endif

#if defined(__NEED_suseconds_t) && !defined(__DEFINED_suseconds_t)
typedef long suseconds_t;
#define __DEFINED_suseconds_t
#endif

#if defined(__NEED_useconds_t) && !defined(__DEFINED_useconds_t)
typedef unsigned int useconds_t;
#define __DEFINED_useconds_t
#endif

#if defined(__NEED_key_t) && !defined(__DEFINED_key_t)
typedef int key_t;
#define __DEFINED_key_t
#endif

#if defined(__NEED_locale_t) && !defined(__DEFINED_locale_t)
typedef struct __locale_struct *locale_t;
#define __DEFINED_locale_t
#endif

#if defined(__NEED_struct_timespec) && !defined(__DEFINED_struct_timespec)
struct timespec {
    long tv_sec; // time_t
    long tv_nsec;
};
#define __DEFINED_struct_timespec
#endif

#if defined(__NEED_pthread_t) && !defined(__DEFINED_pthread_t)
typedef unsigned long pthread_t;
#define __DEFINED_pthread_t
#endif

#if defined(__NEED_pthread_attr_t) && !defined(__DEFINED_pthread_attr_t)
typedef union {
    char __size[56];
    long __align;
} pthread_attr_t;
#define __DEFINED_pthread_attr_t
#endif

#if defined(__NEED_pthread_mutex_t) && !defined(__DEFINED_pthread_mutex_t)
typedef struct {
    volatile int __lock; // 0 free, 1 taken, 2 taken and waited for
    volatile int __owner;
    int __count;           // recursive relocks
    int __type;            // type, robustness and state bits
    void *__next, *__prev; // a robust one's place in its owner's list
    int __pad[2];
} pthread_mutex_t;
#define __DEFINED_pthread_mutex_t
#endif

#if defined(__NEED_pthread_cond_t) && !defined(__DEFINED_pthread_cond_t)
typedef struct {
    volatile int __seq;
    volatile int __waiters;
    int __clock;
    int __shared;
    int __pad[8];
} pthread_cond_t;
#define __DEFINED_pthread_cond_t
#endif

#if defined(__NEED_pthread_rwlock_t) && !defined(__DEFINED_pthread_rwlock_t)
typedef struct {
    volatile int __lock; // guards the rest
    volatile int __seq;  // bumped at each release, for the waiters
    int __readers;
    int __writer; // its tid
    int __waiters;
    int __shared;
    int __pad[8];
} pthread_rwlock_t;
#define __DEFINED_pthread_rwlock_t
#endif

#if defined(__NEED_pthread_barrier_t) && !defined(__DEFINED_pthread_barrier_t)
typedef struct {
    volatile int __lock;
    volatile int __seq;
    unsigned __count, __arrived;
    int __shared;
    int __pad[3];
} pthread_barrier_t;
#define __DEFINED_pthread_barrier_t
#endif

#if defined(__NEED_pthread_attrs) && !defined(__DEFINED_pthread_attrs)
typedef struct {
    unsigned __attr;
} pthread_mutexattr_t;
typedef struct {
    unsigned __attr;
} pthread_condattr_t;
typedef struct {
    unsigned __attr;
} pthread_rwlockattr_t;
typedef struct {
    unsigned __attr;
} pthread_barrierattr_t;
typedef int pthread_once_t;
typedef unsigned pthread_key_t;
typedef volatile int pthread_spinlock_t;
#define __DEFINED_pthread_attrs
#endif

#if defined(__NEED_sigset_t) && !defined(__DEFINED_sigset_t)
typedef struct {
    unsigned long __bits[1]; // signals 1 to 64
} sigset_t;
#define __DEFINED_sigset_t
#endif

#if defined(__NEED_reclen_t) && !defined(__DEFINED_reclen_t)
typedef unsigned short reclen_t;
#define __DEFINED_reclen_t
#endif

#if defined(__NEED_wint_t) && !defined(__DEFINED_wint_t)
typedef unsigned int wint_t;
#define __DEFINED_wint_t
#endif

// A conversion in progress: the bits so far and how many bytes are left,
// or for UTF-16 the low surrogate still to come.
#if defined(__NEED_mbstate_t) && !defined(__DEFINED_mbstate_t)
typedef struct {
    unsigned __bits, __left;
} mbstate_t;
#define __DEFINED_mbstate_t
#endif

#if defined(__NEED_wctype_t) && !defined(__DEFINED_wctype_t)
typedef unsigned long wctype_t;
#define __DEFINED_wctype_t
#endif

#if defined(__NEED_nl_item) && !defined(__DEFINED_nl_item)
typedef int nl_item;
#define __DEFINED_nl_item
#endif

#if defined(__NEED_socklen_t) && !defined(__DEFINED_socklen_t)
typedef unsigned int socklen_t;
#define __DEFINED_socklen_t
#endif

#if defined(__NEED_sa_family_t) && !defined(__DEFINED_sa_family_t)
typedef unsigned short sa_family_t;
#define __DEFINED_sa_family_t
#endif
