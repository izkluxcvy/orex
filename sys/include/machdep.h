#pragma once

#include <stdint.h>

void machdep_init();
void machdep_init_late();

uint64_t machdep_ticks();
