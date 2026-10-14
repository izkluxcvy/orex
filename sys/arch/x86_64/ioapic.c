#include <stdint.h>

#include <printf.h>

#include "apic.h"
#include "ioapic.h"
#include "pmap.h"

#define IOAPIC_PHYS  0xfec0'0000ULL
#define IOAPIC_VBASE 0xffff'ffff'e000'1000ULL

#define IOREGSEL 0x00
#define IOWIN    0x10

#define IOAPIC_VER   0x01
#define IOAPIC_REDIR 0x10

#define REDIR_MASKED 0x10000

static volatile uint32_t *ioapic;
static unsigned           redir_count;

static uint32_t ioapic_read(uint8_t reg) {
    ioapic[IOREGSEL / 4] = reg;
    return ioapic[IOWIN / 4];
}

static void ioapic_write(uint8_t reg, uint32_t value) {
    ioapic[IOREGSEL / 4] = reg;
    ioapic[IOWIN / 4]    = value;
}

void ioapic_init() {
    if (pmap_kenter(IOAPIC_VBASE, IOAPIC_PHYS, PMAP_WRITE | PMAP_PCD) != 0) {
        printf("ioapic: map failed\n");
        return;
    }
    ioapic = (volatile uint32_t *)IOAPIC_VBASE;

    uint32_t ver = ioapic_read(IOAPIC_VER);
    redir_count  = ((ver >> 16) & 0xff) + 1;
    for (unsigned i = 0; i < redir_count; i++) {
        ioapic_write(IOAPIC_REDIR + 2 * i, REDIR_MASKED);
    }

    printf("ioapic: version %lx, %u inputs\n", (unsigned long)(ver & 0xff),
           redir_count);
}

void ioapic_route_isa(unsigned irq, uint8_t vector) {
    if (!ioapic || irq >= redir_count) {
        return;
    }
    ioapic_write(IOAPIC_REDIR + 2 * irq + 1, apic_id() << 24);
    ioapic_write(IOAPIC_REDIR + 2 * irq, vector);
}
