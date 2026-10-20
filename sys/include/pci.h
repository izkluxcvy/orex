#pragma once

#include <stddef.h>
#include <stdint.h>

struct pci_dev {
    uint8_t  bus, slot, func;
    uint16_t vendor, device;
    uint8_t class, subclass;
    uint64_t bar[6];
    uint64_t bar_len[6];
};

void            pci_init();
struct pci_dev *pci_at(int index);

uint32_t pci_read32(const struct pci_dev *d, uint8_t off);
uint16_t pci_read16(const struct pci_dev *d, uint8_t off);
uint8_t  pci_read8(const struct pci_dev *d, uint8_t off);
void     pci_write32(const struct pci_dev *d, uint8_t off, uint32_t v);
void     pci_write16(const struct pci_dev *d, uint8_t off, uint16_t v);

uint8_t pci_find_cap(const struct pci_dev *d, uint8_t id, uint8_t from);
void    pci_enable(const struct pci_dev *d);
void   *pci_map_bar(const struct pci_dev *d, int bar, uint64_t off, size_t len);
int     pci_msix_route(const struct pci_dev *d, unsigned entry, uint8_t vector);
