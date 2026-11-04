#pragma once

extern unsigned long *__auxv;

unsigned long __getauxval(unsigned long type);

#include <stdint.h>

uint64_t __random_u64();

__attribute__((weak)) void __cxa_thread_finalize();
