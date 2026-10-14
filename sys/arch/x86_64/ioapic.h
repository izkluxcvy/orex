#pragma once

#include <stdint.h>

void ioapic_init();

void ioapic_route_isa(unsigned irq, uint8_t vector);
