#include <stdint.h>

#include <cpu.h>
#include <panic.h>
#include <printf.h>
#include <spinlock.h>

#include "cpufunc.h"
#include "irq.h"

#define STUCK_CYCLES (10ull * 1000 * 1000 * 1000)

void spin_lock_init(struct spinlock *lk, const char *name) {
    lk->name = name;
    lk->held = 0;
    lk->cpu  = -1;
}

static const char *name_of(const struct spinlock *lk) {
    return lk->name ? lk->name : "?";
}

static void stuck(struct spinlock *lk) {
    struct cpu *c = curcpu();
    printf("spinlock: cpu %d stuck on %s, held by cpu %d; holding", c->id,
           name_of(lk), lk->cpu);
    for (int i = 0; i < c->nlocks && i < 8; i++) {
        printf(" %s", c->locks[i]);
    }

    int o = lk->cpu;
    if (o >= 0 && o < MAX_CPUS) {
        printf("; cpu %d holds", o);
        for (int i = 0; i < cpus[o].nlocks && i < 8; i++) {
            printf(" %s", cpus[o].locks[i]);
        }
    }
    printf("\n");
}

uint64_t spin_lock_irqsave(struct spinlock *lk) {
    uint64_t    flags = irq_save();
    struct cpu *c     = curcpu();

    if (lk->held && lk->cpu == c->id) {
        panic("spinlock: %s acquired recursively on cpu %d", name_of(lk),
              c->id);
    }

    if (__atomic_exchange_n(&lk->held, 1, __ATOMIC_ACQUIRE)) {
        uint64_t start = rdtsc();
        int      told  = 0;
        do {
            while (__atomic_load_n(&lk->held, __ATOMIC_RELAXED)) {
                __asm__ __volatile__("pause");
                if (!told && rdtsc() - start > STUCK_CYCLES) {
                    stuck(lk);
                    told = 1;
                }
            }
        } while (__atomic_exchange_n(&lk->held, 1, __ATOMIC_ACQUIRE));
    }
    lk->cpu = c->id;
    if (c->nlocks < 8) {
        c->locks[c->nlocks] = name_of(lk);
    }
    c->nlocks++;
    return flags;
}

void spin_unlock(struct spinlock *lk) {
    struct cpu *c = curcpu();
    const char *n = name_of(lk);
    for (int i = (c->nlocks < 8 ? c->nlocks : 8) - 1; i >= 0; i--) {
        if (c->locks[i] == n) {
            c->locks[i] = c->locks[c->nlocks > 8 ? 7 : c->nlocks - 1];
            break;
        }
    }
    c->nlocks--;
    lk->cpu = -1;
    __atomic_store_n(&lk->held, 0, __ATOMIC_RELEASE);
}

void spin_unlock_irqrestore(struct spinlock *lk, uint64_t flags) {
    spin_unlock(lk);
    irq_restore(flags);
}

int spin_held(const struct spinlock *lk) {
    return lk->held && lk->cpu == curcpu()->id;
}
