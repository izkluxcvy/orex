#pragma once

#include <stdint.h>

void     apic_init();
void     apic_eoi();
uint32_t apic_id();
void     apic_timer_init(uint32_t hz);
uint32_t apic_timer_hz();

extern uint64_t tsc_hz;
