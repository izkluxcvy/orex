#pragma once

#include <stdint.h>

struct vmspace;

#define FUTEX_WAIT             0
#define FUTEX_WAKE             1
#define FUTEX_REQUEUE          3
#define FUTEX_CMP_REQUEUE      4
#define FUTEX_WAIT_BITSET      9
#define FUTEX_WAKE_BITSET      10
#define FUTEX_PRIVATE_FLAG     128
#define FUTEX_CLOCK_REALTIME   256
#define FUTEX_BITSET_MATCH_ANY 0xffffffffU

int futex_wake(struct vmspace *vm, uintptr_t addr, int n);
struct thread;
void futex_exit_robust(struct thread *t);
long sys_futex(uint32_t *uaddr, long op, uint32_t val, const void *utimeout,
               uint32_t *uaddr2, uint32_t val3);
