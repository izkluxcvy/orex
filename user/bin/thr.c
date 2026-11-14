#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

static void say(const char *s) { write(1, s, strlen(s)); }

static void num(long v) {
    char buf[24];
    char *p = buf + sizeof(buf);
    int neg = v < 0;
    unsigned long u = neg ? -(unsigned long)v : (unsigned long)v;
    do {
        *--p = (char)('0' + u % 10);
    } while (u /= 10);
    if (neg) {
        *--p = '-';
    }
    write(1, p, (size_t)(buf + sizeof(buf) - p));
}

#define THREADS 4
#define ROUNDS  100000

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static long counter;
static _Thread_local long mine;

static void *work(void *arg) {
    long id = (long)arg;
    for (int i = 0; i < ROUNDS; i++) {
        pthread_mutex_lock(&lock);
        volatile int tmp = 1;
        for (int j = 0; j < 100; j++) {
            tmp *= 2;
            tmp /= 2;
        }
        counter += tmp;
        pthread_mutex_unlock(&lock);
        mine++;
    }
    close(-1);
    return (void *)(id * 100 + errno + mine / ROUNDS);
}

int main() {
    pthread_t t[THREADS];
    for (long i = 0; i < THREADS; i++) {
        pthread_create(&t[i], nullptr, work, (void *)i);
    }
    open("/nonexistent", O_RDONLY);
    for (long i = 0; i < THREADS; i++) {
        void *r;
        pthread_join(t[i], &r);
        say("thr: thread ");
        num(i);
        say(" returned ");
        num((long)r);
        say("\n");
    }
    say("thr: the counter is ");
    num(counter);
    say(", main's errno is ");
    num(errno);
    say(", main's count is ");
    num(mine);
    say("\n");
    return 0;
}
