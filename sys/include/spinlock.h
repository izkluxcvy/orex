#pragma once

#include <stdint.h>

struct spinlock {
    const char  *name;
    volatile int held;
    int          cpu;
};

void spin_lock_init(struct spinlock *lk, const char *name);

uint64_t spin_lock_irqsave(struct spinlock *lk);
void     spin_unlock_irqrestore(struct spinlock *lk, uint64_t flags);

void spin_unlock(struct spinlock *lk);

int spin_held(const struct spinlock *lk);
