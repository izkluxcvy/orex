#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_time_t
#define __NEED_clock_t
#define __NEED_clockid_t
#define __NEED_timer_t
#define __NEED_pid_t
#define __NEED_locale_t
#define __NEED_struct_timespec
#include <bits/alltypes.h>

#define __need_NULL
#include <stddef.h>

#define CLOCKS_PER_SEC 1000000L
#define TIME_UTC       1

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7

#define TIMER_ABSTIME 1

struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst;
    long tm_gmtoff; // POSIX 2024: seconds east of UTC
    const char *tm_zone;
};

struct itimerspec {
    struct timespec it_interval, it_value;
};

struct sigevent;

extern char *tzname[2];
extern long timezone; // seconds west of UTC, in standard time
extern int daylight;
extern int getdate_err;
struct tm *getdate(const char *s);

char *asctime(const struct tm *tm);
char *asctime_r(const struct tm *__restrict tm, char *__restrict buf);
clock_t clock(void);
int clock_getcpuclockid(pid_t pid, clockid_t *clock);
int clock_getres(clockid_t clock, struct timespec *res);
int clock_gettime(clockid_t clock, struct timespec *ts);
int clock_nanosleep(clockid_t clock, int flags, const struct timespec *req,
                    struct timespec *rem);
int clock_settime(clockid_t clock, const struct timespec *ts);
char *ctime(const time_t *t);
char *ctime_r(const time_t *t, char *buf);
double difftime(time_t a, time_t b);
struct tm *gmtime(const time_t *t);
struct tm *gmtime_r(const time_t *__restrict t, struct tm *__restrict tm);
struct tm *localtime(const time_t *t);
struct tm *localtime_r(const time_t *__restrict t, struct tm *__restrict tm);
time_t mktime(struct tm *tm);
int nanosleep(const struct timespec *req, struct timespec *rem);
size_t strftime(char *__restrict s, size_t max, const char *__restrict fmt,
                const struct tm *__restrict tm);
size_t strftime_l(char *__restrict s, size_t max, const char *__restrict fmt,
                  const struct tm *__restrict tm, locale_t loc);
char *strptime(const char *__restrict s, const char *__restrict fmt,
               struct tm *__restrict tm);
time_t time(time_t *t);
time_t timegm(struct tm *tm);
int timer_create(clockid_t clock, struct sigevent *__restrict sev,
                 timer_t *__restrict id);
int timer_delete(timer_t id);
int timer_getoverrun(timer_t id);
int timer_gettime(timer_t id, struct itimerspec *cur);
int timer_settime(timer_t id, int flags,
                  const struct itimerspec *__restrict value,
                  struct itimerspec *__restrict old);
int timespec_get(struct timespec *ts, int base);
int timespec_getres(struct timespec *ts, int base);
void tzset(void);

#ifdef __cplusplus
}
#endif
