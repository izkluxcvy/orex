#include "../lib/ulib.h"

static void say(const char *what, long n) {
    puts(what);
    putnum(n);
    puts("\n");
}

#define SYS_pause         34
#define SYS_nanosleep     35
#define SYS_setitimer     38
#define SYS_times         100
#define SYS_clock_gettime 228
#define SIGALRM           14

struct timespec {
    long sec, nsec;
};

static long now_ms() {
    struct timespec ts;
    syscall4(SYS_clock_gettime, 1, (long)&ts, 0, 0);
    return ts.sec * 1000 + ts.nsec / 1000000;
}

static long sleep_ms(long ms, struct timespec *left) {
    struct timespec ts = {ms / 1000, ms % 1000 * 1000000};
    return syscall4(SYS_nanosleep, (long)&ts, (long)left, 0, 0);
}

static void alarm_us(long first, long interval) {
    long it[4] = {0, interval, 0, first};
    syscall4(SYS_setitimer, 0, (long)it, 0, 0);
}

static volatile int alarms;

static void on_alarm(int sig) {
    (void)sig;
    alarms++;
}

int main() {
    struct timespec ts;
    syscall4(SYS_clock_gettime, 0, (long)&ts, 0, 0);
    say("init: seconds since 1970: ", ts.sec);

    long t0 = now_ms();
    sleep_ms(250, nullptr);
    say("init: asked to sleep 250 ms, and slept ", now_ms() - t0);

    signal(SIGALRM, on_alarm);
    t0 = now_ms();
    alarm_us(100000, 100000);
    while (alarms < 5) {
        syscall4(SYS_pause, 0, 0, 0, 0);
    }
    alarm_us(0, 0);
    say("init: five alarms 100 ms apart took ", now_ms() - t0);

    alarm_us(300000, 0);
    long r = sleep_ms(1000, &ts);
    say("init: a one-second sleep, an alarm at 300 ms: it returned ", r);
    say("init: with milliseconds left: ", ts.sec * 1000 + ts.nsec / 1000000);

    long tms[4];
    t0 = now_ms();
    sleep_ms(200, nullptr);
    while (now_ms() - t0 < 500) {
    }
    syscall4(SYS_times, (long)tms, 0, 0, 0);
    say("init: 200 ms asleep and 300 busy; hundredths of a second in user "
        "mode: ",
        tms[0]);
    say("init: and in the kernel: ", tms[1]);
    return 0;
}
