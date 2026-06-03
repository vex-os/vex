#include <amd64/pmio.h>
#include <bus/pci.h>
#include <init/initcall.h>

// PCI configuration space access via cf8 CF8/CFC I/O ports.
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

static uint8_t cf8_read8(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    return pmread8(PCI_CFC + (offset & 3));
}

static uint16_t cf8_read16(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    return pmread16(PCI_CFC + (offset & 2));
}

static uint32_t cf8_read32(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    return pmread32(PCI_CFC);
}

static void cf8_write8(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint8_t value)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    pmwrite8(PCI_CFC + (offset & 3), value);
}

static void cf8_write16(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint16_t value)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    pmwrite16(PCI_CFC + (offset & 2), value);
}

static void cf8_write32(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint32_t value)
{
    pmwrite32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    pmwrite32(PCI_CFC, value);
}

static const struct pci_bus_ops cf8_ops = {
    .read8 = &cf8_read8,
    .read16 = &cf8_read16,
    .read32 = &cf8_read32,
    .write8 = &cf8_write8,
    .write16 = &cf8_write16,
    .write32 = &cf8_write32,
};

static struct pci_bus cf8_bus = {
    .ops = &cf8_ops,
    .identifier = "CF8",
    .segment = 0,
    .index = 0,
};

define_initcall(amd64_pci_cf8, INITCALL_STAGE_BSP_BUS, INITCALL_ORDER_FIRST)
{
    pci_bus_register(&cf8_bus);
}
