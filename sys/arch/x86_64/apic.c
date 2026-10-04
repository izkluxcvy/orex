#include <stdint.h>

#include <printf.h>

#include "apic.h"
#include "cpufunc.h"
#include "io.h"
#include "irq.h"
#include "msr.h"
#include "pmap.h"

#define LAPIC_VBASE 0xFFFF'FFFF'E000'0000ull

#define APIC_BASE_ENABLE    (1ull << 11)
#define APIC_BASE_ADDR_MASK 0x000F'FFFF'FFFF'F000ull

#define LAPIC_ID        0x020
#define LAPIC_VERSION   0x030
#define LAPIC_TPR       0x080
#define LAPIC_EOI       0x0B0
#define LAPIC_SVR       0x0F0
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERROR 0x370
#define LAPIC_TIMER_ICR 0x380 // initial count
#define LAPIC_TIMER_CCR 0x390 // current count
#define LAPIC_TIMER_DCR 0x3E0 // divide configuration

#define SVR_ENABLE      0x100
#define LVT_MASKED      0x10000
#define LVT_PERIODIC    0x20000
#define TIMER_DIVIDE_16 0x3

#define PIT_FREQ_HZ   1193182u
#define PIT_CH2_DATA  0x42
#define PIT_COMMAND   0x43
#define PIT_GATE_PORT 0x61
#define CALIBRATE_MS  50 // the PIT's 16 bits hold up to 54 ms

static volatile uint8_t *lapic;
static uint32_t          timer_hz;
static uint32_t          timer_ticks_per_sec;

static inline void lapic_write(unsigned reg, uint32_t value) {
    *(volatile uint32_t *)(lapic + reg) = value;
}

static inline uint32_t lapic_read(unsigned reg) {
    return *(volatile uint32_t *)(lapic + reg);
}

void apic_eoi() { lapic_write(LAPIC_EOI, 0); }

uint32_t apic_id() { return lapic_read(LAPIC_ID) >> 24; }

static void pic_disable() {
    outb(0x20, 0x11); // ICW1: start initialization
    outb(0xA0, 0x11);
    outb(0x21, 0x20); // ICW2: master vector offset
    outb(0xA1, 0x28); // ICW2: slave vector offset
    outb(0x21, 0x04); // ICW3: slave on IRQ2
    outb(0xA1, 0x02);
    outb(0x21, 0x01); // ICW4: 8086 mode
    outb(0xA1, 0x01);
    outb(0x21, 0xFF); // mask all IRQs
    outb(0xA1, 0xFF);
}

static void lapic_setup() {
    wrmsr(MSR_IA32_APIC_BASE, rdmsr(MSR_IA32_APIC_BASE) | APIC_BASE_ENABLE);

    lapic_write(LAPIC_SVR, SVR_ENABLE | IRQ_VECTOR_SPURIOUS);
    lapic_write(LAPIC_TPR, 0);
    lapic_write(LAPIC_LVT_TIMER, LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT0, LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, LVT_MASKED);
}

void apic_init() {
    pic_disable();

    uint64_t base = rdmsr(MSR_IA32_APIC_BASE);
    uint64_t phys = base & APIC_BASE_ADDR_MASK;

    if (pmap_kenter(LAPIC_VBASE, phys, PMAP_WRITE | PMAP_PCD) != 0) {
        printf("apic: failed to map LAPIC at 0x%lx\n", phys);
        return;
    }
    lapic = (volatile uint8_t *)LAPIC_VBASE;

    lapic_setup();

    printf(
        "apic: LAPIC ID %lu version %lu at 0x%lx\n", (unsigned long)apic_id(),
        (unsigned long)(lapic_read(LAPIC_VERSION) & 0xFF), (unsigned long)phys);
}

uint64_t tsc_hz;

static uint32_t calibrate() {
    uint32_t pit_ticks = (PIT_FREQ_HZ * CALIBRATE_MS) / 1000;

    // Set up PIT channel 2 to the gate we can poll, with the speaker off
    uint8_t gate = (uint8_t)((inb(PIT_GATE_PORT) & ~0x02) | 0x01);
    outb(PIT_GATE_PORT, (uint8_t)(gate & ~0x01)); // hold gate low
    outb(PIT_COMMAND, 0xB2); // channel 2, lo/hi byte, mode 1, binary
    outb(PIT_CH2_DATA, (uint8_t)(pit_ticks & 0xFF));
    outb(PIT_CH2_DATA, (uint8_t)(pit_ticks >> 8));

    lapic_write(LAPIC_TIMER_DCR, TIMER_DIVIDE_16);

    outb(PIT_GATE_PORT, gate);
    lapic_write(LAPIC_TIMER_ICR, 0xFFFF'FFFF);
    uint64_t t0 = rdtsc();

    while (!(inb(PIT_GATE_PORT) & 0x20)) { // wait for OUT2 to go high
    }
    tsc_hz = (rdtsc() - t0) * (1000 / CALIBRATE_MS);

    lapic_write(LAPIC_LVT_TIMER, LVT_MASKED);
    uint32_t elapsed = 0xFFFF'FFFF - lapic_read(LAPIC_TIMER_CCR);

    return elapsed * (1000 / CALIBRATE_MS);
}

void apic_timer_init(uint32_t hz) {
    if (!lapic) {
        return;
    }

    timer_hz            = hz;
    timer_ticks_per_sec = calibrate();

    uint32_t count = timer_ticks_per_sec / hz;
    if (count == 0) {
        count = 1;
    }

    lapic_write(LAPIC_TIMER_DCR, TIMER_DIVIDE_16);
    lapic_write(LAPIC_LVT_TIMER, LVT_PERIODIC | IRQ_VECTOR_TIMER);
    lapic_write(LAPIC_TIMER_ICR, count);

    printf("apic: timer %lu Hz (%lu ticks/s, count %lu), tsc %lu Hz\n",
           (unsigned long)hz, (unsigned long)timer_ticks_per_sec,
           (unsigned long)count, (unsigned long)tsc_hz);
}

uint32_t apic_timer_hz() { return timer_hz; }
