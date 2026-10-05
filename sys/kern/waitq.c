#include <stdint.h>

#include <sched.h>
#include <spinlock.h>
#include <thread.h>
#include <waitq.h>

void waitq_init(struct waitq *wq) {
    wq->head = nullptr;
    wq->tail = nullptr;
}

void waitq_remove(struct waitq *wq, struct thread *t) {
    if (t->prev) {
        t->prev->next = t->next;
    } else {
        wq->head = t->next;
    }
    if (t->next) {
        t->next->prev = t->prev;
    } else {
        wq->tail = t->prev;
    }
    t->next = nullptr;
    t->prev = nullptr;
}

void waitq_sleep(struct waitq *wq) {
    struct thread *t = curthread;

    t->state = THREAD_BLOCKED;
    t->next  = nullptr;
    t->prev  = wq->tail;
    if (wq->tail) {
        wq->tail->next = t;
    } else {
        wq->head = t;
    }
    wq->tail = t;

    schedule();
}

int waitq_sleep_intr(struct waitq *wq) {
    curthread->sleep_wq = wq;
    waitq_sleep(wq);
    curthread->sleep_wq = nullptr;
    return 0;
}

static struct thread *waitq_take_highest(struct waitq *wq) {
    struct thread *best = wq->head;
    if (!best) {
        return nullptr;
    }
    for (struct thread *t = best->next; t; t = t->next) {
        if (t->priority > best->priority) {
            best = t;
        }
    }

    waitq_remove(wq, best);
    return best;
}

void waitq_wakeup_one(struct waitq *wq) {
    struct thread *t = waitq_take_highest(wq);
    if (t) {
        sched_enqueue(t);
    }
}

void waitq_wakeup_all(struct waitq *wq) {
    while (wq->head) {
        struct thread *t = wq->head;
        waitq_remove(wq, t);
        sched_enqueue(t);
    }
}
