#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_pid_t
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_id_t
#define __NEED_off_t
#define __NEED_mode_t
#define __NEED_dev_t
#define __NEED_ino_t
#define __NEED_nlink_t
#define __NEED_blksize_t
#define __NEED_blkcnt_t
#define __NEED_fsblkcnt_t
#define __NEED_fsfilcnt_t
#define __NEED_time_t
#define __NEED_clock_t
#define __NEED_clockid_t
#define __NEED_timer_t
#define __NEED_suseconds_t
#define __NEED_key_t
#define __NEED_reclen_t
#define __NEED_pthread_t
#define __NEED_pthread_attr_t
#define __NEED_pthread_mutex_t
#define __NEED_pthread_cond_t
#define __NEED_pthread_rwlock_t
#define __NEED_pthread_barrier_t
#define __NEED_pthread_attrs
#include <bits/alltypes.h>

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef long quad_t;
typedef unsigned long u_quad_t;
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long u_int64_t;
typedef char *caddr_t;

#ifdef __cplusplus
}
#endif
