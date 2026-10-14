#include "../lib/ulib.h"

int main() {
    puts("init: press some keys\n");
    while (1) {
        syscall4(34, 0, 0, 0, 0);
    }
}
