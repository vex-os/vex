// SPDX-License-Identifier: BSD-3-Clause
#ifndef DE230BAC_AA17_4780_B3C2_71F7DA67E522
#define DE230BAC_AA17_4780_B3C2_71F7DA67E522
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

struct pci_bus;
struct pci_device;

struct pci_bus_ops {
    uint8_t (*read8)(struct pci_bus* restrict bus, uint8_t device_index, uint8_t function, uint16_t offset);
    uint16_t (*read16)(struct pci_bus* restrict bus, uint8_t device_index, uint8_t function, uint16_t offset);
    uint32_t (*read32)(struct pci_bus* restrict bus, uint8_t device_index, uint8_t function, uint16_t offset);
    void (*write8)(struct pci_bus* restrict bus, uint8_t device_index, uint8_t function, uint16_t offset, uint8_t value);
    void (*write16)(struct pci_bus* restrict bus, uint8_t device_index, uint8_t function, uint16_t offset, uint16_t value);
    void (*write32)(struct pci_bus* restrict bus, uint8_t device_index, uint8_t function, uint16_t offset, uint32_t value);
};

struct pci_bus {
    struct pci_bus* next;
    const struct pci_bus_ops* ops;
    char identity[64];
    uint16_t segment;
    uint8_t bus_index;
    void* privdata;
};

struct pci_device {
    struct pci_bus* bus;
    uint8_t device_index;
    uint8_t function;
};

extern int register_pci_bus(struct pci_bus* restrict bus);

extern struct pci_device* pci_open(uint16_t segment, uint8_t bus_index, uint8_t device_index, uint8_t function) __nodiscard;
extern void pci_close(struct pci_device* restrict device_index);

extern uint8_t pci_read8(struct pci_device* restrict device_index, uint16_t offset);
extern uint16_t pci_read16(struct pci_device* restrict device_index, uint16_t offset);
extern uint32_t pci_read32(struct pci_device* restrict device_index, uint16_t offset);

extern void pci_write8(struct pci_device* restrict device_index, uint16_t offset, uint8_t value);
extern void pci_write16(struct pci_device* restrict device_index, uint16_t offset, uint16_t value);
extern void pci_write32(struct pci_device* restrict device_index, uint16_t offset, uint32_t value);

#endif /* DE230BAC_AA17_4780_B3C2_71F7DA67E522 */
