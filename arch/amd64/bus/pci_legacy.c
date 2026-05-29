// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/pmio.h>
#include <bus/pci.h>
#include <init/initcall.h>
#include <stdint.h>

// PCI configuration space access via legacy CF8/CFC I/O ports.
// Covers segment 0 only; other segments require ECAM (MCFG).

#define PCI_CF8 UINT16_C(0x0CF8)
#define PCI_CFC UINT16_C(0x0CFC)

static uint32_t cf8_addr(struct pci_bus* bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    uint32_t result = UINT32_C(0);
    result |= UINT32_C(1) << 31;
    result |= (uint32_t)(bus->index) << 16;
    result |= (uint32_t)(device_index & 0x1F) << 11;
    result |= (uint32_t)(function & 0x07) << 8;
    result |= (uint32_t)(offset & 0xFC);
    return result;
}

static uint8_t legacy_read8(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    return pmread8(PCI_CFC + (offset & 3));
}

static uint16_t legacy_read16(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    return pmread16(PCI_CFC + (offset & 2));
}

static uint32_t legacy_read32(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    return pmread32(PCI_CFC);
}

static void legacy_write8(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint8_t value)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    pmwrite8(PCI_CFC + (offset & 3), value);
}

static void legacy_write16(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint16_t value)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    pmwrite16(PCI_CFC + (offset & 2), value);
}

static void legacy_write32(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint32_t value)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    pmwrite32(PCI_CFC, value);
}

static const struct pci_bus_ops legacy_ops = {
    .read8 = &legacy_read8,
    .read16 = &legacy_read16,
    .read32 = &legacy_read32,
    .write8 = &legacy_write8,
    .write16 = &legacy_write16,
    .write32 = &legacy_write32,
};

static struct pci_bus legacy_bus = {
    .ops = &legacy_ops,
    .identifier = "cf8_legacy",
    .segment = 0,
    .index = 0,
};

define_initcall(amd64_pci_legacy, INITCALL_STAGE_BSP_BUS, INITCALL_ORDER_FIRST)
{
    register_pci_bus(&legacy_bus);
}
