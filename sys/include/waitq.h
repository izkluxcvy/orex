#pragma once

struct thread;

struct waitq {
    struct thread *head;
    struct thread *tail;
};

void waitq_init(struct waitq *wq);

void waitq_sleep(struct waitq *wq);

void waitq_remove(struct waitq *wq, struct thread *t);

void waitq_wakeup_one(struct waitq *wq);
void waitq_wakeup_all(struct waitq *wq);
