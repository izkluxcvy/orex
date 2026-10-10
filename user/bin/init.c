#include "../lib/ulib.h"

static double leibniz(long terms) {
    double sum = 0, sign = 1;
    for (long k = 0; k < terms; k++) {
        sum += sign / (double)(2 * k + 1);
        sign = -sign;
    }
    return 4 * sum;
}

static double newton(long rounds) {
    double x = 1;
    for (long i = 0; i < rounds; i++) {
        x = 1;
        for (int k = 0; k < 8; k++) {
            x = (x + 2 / x) / 2;
        }
    }
    return x;
}

static void say(const char *what, double v) {
    puts(what);
    putnum((long)(v * 1000000));
    puts(" millionths\n");
}

int main() {
    if (fork() == 0) {
        say("one: pi is about ", leibniz(30000000));
        _exit(0);
    }
    if (fork() == 0) {
        say("two: the square root of 2 is about ", newton(10000000));
        _exit(0);
    }
    double mine = 2.5;
    while (waitpid(-1, nullptr, 0) > 0) {
        mine *= 2;
    }
    say("init: and 2.5 doubled twice is ", mine);
    return 0;
}
