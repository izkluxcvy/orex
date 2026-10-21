#include <stddef.h>
#include <stdint.h>

#include <blk.h>

#define MAX_DEVS 32

static struct blkdev *devs[MAX_DEVS];
static int            ndevs;

void blk_register(struct blkdev *dev) {
    if (ndevs < MAX_DEVS) {
        devs[ndevs++] = dev;
    }
}

struct blkdev *blk_at(int index) {
    return index >= 0 && index < ndevs ? devs[index] : nullptr;
}

struct blkdev *blk_find(const char *name) {
    for (int i = 0; i < ndevs; i++) {
        const char *a = devs[i]->name, *b = name;
        while (*a && *a == *b) {
            a++, b++;
        }
        if (*a == *b) {
            return devs[i];
        }
    }
    return nullptr;
}
