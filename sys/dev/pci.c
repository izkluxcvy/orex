#include <stddef.h>
#include <stdint.h>

#include <pci.h>
#include <printf.h>

#include "apic.h"
#include "io.h"
#include "pmap.h"

#define CONFIG_ADDR 0xcf8
#define CONFIG_DATA 0xcfc
#define MAX_DEVS    64

#define PCI_COMMAND    0x04
#define PCI_STATUS     0x06
#define PCI_HEADER     0x0e
#define PCI_BAR0       0x10
#define PCI_CAP_PTR    0x34
#define CMD_MEMORY     0x2
#define CMD_MASTER     0x4
#define CMD_INTX_OFF   0x400
#define STATUS_CAPS    0x10
#define CAP_MSIX       0x11
#define MSIX_ENABLE    0x8000
#define MSIX_FUNC_MASK 0x4000

static struct pci_dev devs[MAX_DEVS];
static int            ndevs;

static uint32_t addr_of(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    return 0x8000'0000U | (uint32_t)bus << 16 | (uint32_t)slot << 11 |
           (uint32_t)func << 8 | (off & 0xfc);
}

static uint32_t cfg_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    outl(CONFIG_ADDR, addr_of(bus, slot, func, off));
    return inl(CONFIG_DATA);
}

uint32_t pci_read32(const struct pci_dev *d, uint8_t off) {
    return cfg_read(d->bus, d->slot, d->func, off);
}
uint16_t pci_read16(const struct pci_dev *d, uint8_t off) {
    return cfg_read(d->bus, d->slot, d->func, off) >> ((off & 2) * 8);
}
uint8_t pci_read8(const struct pci_dev *d, uint8_t off) {
    return cfg_read(d->bus, d->slot, d->func, off) >> ((off & 3) * 8);
}

void pci_write32(const struct pci_dev *d, uint8_t off, uint32_t v) {
    outl(CONFIG_ADDR, addr_of(d->bus, d->slot, d->func, off));
    outl(CONFIG_DATA, v);
}

void pci_write16(const struct pci_dev *d, uint8_t off, uint16_t v) {
    uint32_t old   = pci_read32(d, off);
    int      shift = (off & 2) * 8;
    pci_write32(d, off, (old & ~(0xffffU << shift)) | (uint32_t)v << shift);
}

static void size_bars(struct pci_dev *d) {
    uint16_t cmd = pci_read16(d, PCI_COMMAND);
    pci_write16(d, PCI_COMMAND, cmd & ~(CMD_MEMORY | 1));
    for (int i = 0; i < 6; i++) {
        uint8_t  off = PCI_BAR0 + i * 4;
        uint32_t lo  = pci_read32(d, off);
        if (lo & 1) {
            continue; // I/O space
        }
        int      wide = (lo & 6) == 4;
        uint32_t hi   = wide && i < 5 ? pci_read32(d, off + 4) : 0;
        pci_write32(d, off, 0xffff'ffff);
        uint64_t mask = pci_read32(d, off) & ~0xfU;
        pci_write32(d, off, lo);
        if (wide && i < 5) {
            pci_write32(d, off + 4, 0xffff'ffff);
            mask |= (uint64_t)pci_read32(d, off + 4) << 32;
            pci_write32(d, off + 4, hi);
        } else {
            mask |= 0xffff'ffff'0000'0000ULL;
        }
        uint64_t base = ((uint64_t)hi << 32) | (lo & ~0xfU);
        if (base && mask) {
            d->bar[i]     = base;
            d->bar_len[i] = ~mask + 1;
        }
        i += wide;
    }
    pci_write16(d, PCI_COMMAND, cmd);
}

static void probe(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t id = cfg_read(bus, slot, func, 0);
    if ((id & 0xffff) == 0xffff || ndevs == MAX_DEVS) {
        return;
    }
    struct pci_dev *d = &devs[ndevs++];
    *d                = (struct pci_dev){.bus    = bus,
                                         .slot   = slot,
                                         .func   = func,
                                         .vendor = (uint16_t)id,
                                         .device = (uint16_t)(id >> 16)};
    uint32_t class    = cfg_read(bus, slot, func, 8);
    d->class          = (uint8_t)(class >> 24);
    d->subclass       = (uint8_t)(class >> 16);
    if ((pci_read8(d, PCI_HEADER) & 0x7f) == 0) {
        size_bars(d);
    }
}

void pci_init() {
    for (unsigned bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            if ((cfg_read((uint8_t)bus, slot, 0, 0) & 0xffff) == 0xffff) {
                continue;
            }
            int multi =
                (cfg_read((uint8_t)bus, slot, 0, PCI_HEADER & 0xfc) >> 16) &
                0x80;
            for (uint8_t func = 0; func < (multi ? 8 : 1); func++) {
                probe((uint8_t)bus, slot, func);
            }
        }
    }
    printf("pci: %d devices\n", ndevs);
}

struct pci_dev *pci_at(int index) {
    return index >= 0 && index < ndevs ? &devs[index] : nullptr;
}

uint8_t pci_find_cap(const struct pci_dev *d, uint8_t id, uint8_t from) {
    if (!(pci_read16(d, PCI_STATUS) & STATUS_CAPS)) {
        return 0;
    }
    uint8_t at = from ? pci_read8(d, from + 1) : pci_read8(d, PCI_CAP_PTR);
    for (int n = 0; at && n < 48; n++, at = pci_read8(d, at + 1)) {
        at &= 0xfc;
        if (pci_read8(d, at) == id) {
            return at;
        }
    }
    return 0;
}

void pci_enable(const struct pci_dev *d) {
    pci_write16(d, PCI_COMMAND,
                pci_read16(d, PCI_COMMAND) | CMD_MEMORY | CMD_MASTER);
}

void *pci_map_bar(const struct pci_dev *d, int bar, uint64_t off, size_t len) {
    if (bar < 0 || bar > 5 || !d->bar[bar] || off + len > d->bar_len[bar]) {
        return nullptr;
    }
    return kmap_mmio(d->bar[bar] + off, len);
}

int pci_msix_route(const struct pci_dev *d, unsigned entry, uint8_t vector) {
    uint8_t cap = pci_find_cap(d, CAP_MSIX, 0);
    if (!cap) {
        return -1;
    }
    uint16_t ctl   = pci_read16(d, cap + 2);
    uint32_t table = pci_read32(d, cap + 4);
    if (entry > (ctl & 0x7ffU)) {
        return -1;
    }
    volatile uint32_t *e =
        pci_map_bar(d, (int)(table & 7), (table & ~7U) + entry * 16, 16);
    if (!e) {
        return -1;
    }
    e[0] = 0xfee0'0000U | apic_id() << 12;
    e[1] = 0;
    e[2] = vector;
    e[3] = 0;
    pci_write16(d, cap + 2, (uint16_t)((ctl | MSIX_ENABLE) & ~MSIX_FUNC_MASK));
    pci_write16(d, PCI_COMMAND, pci_read16(d, PCI_COMMAND) | CMD_INTX_OFF);
    return 0;
}
