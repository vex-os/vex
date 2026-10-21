#include <arch/io.h>
#include <vex/initcall.h>
#include <vex/pci.h>
#include <vex/spinlock.h>

// Covers segment 0 only; other segments require ECAM (MCFG)

#define PCI_CF8 UINT16_C(0x0CF8)
#define PCI_CFC UINT16_C(0x0CFC)

static spinlock_t cf8_lock = SPINLOCK_INIT;

static uint32_t cf8_addr(pci_bus_t* bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    uint32_t result = UINT32_C(0);
    result |= UINT32_C(1) << 31;
    result |= (bus->index) << 16;
    result |= (device_index & 0x1F) << 11;
    result |= (function & 0x07) << 8;
    result |= (offset & 0xFC);
    return result;
}

static uint8_t cf8_read8(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    bool intr_state = spinlock_acquire_intr(&cf8_lock);
    port_write32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    uint8_t value = port_read8(PCI_CFC + (offset & 3));
    spinlock_release_intr(&cf8_lock, intr_state);
    return value;
}

static uint16_t cf8_read16(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    bool intr_state = spinlock_acquire_intr(&cf8_lock);
    port_write32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    uint16_t value = port_read16(PCI_CFC + (offset & 2));
    spinlock_release_intr(&cf8_lock, intr_state);
    return value;
}

static uint32_t cf8_read32(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset)
{
    bool intr_state = spinlock_acquire_intr(&cf8_lock);
    port_write32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    uint32_t value = port_read32(PCI_CFC);
    spinlock_release_intr(&cf8_lock, intr_state);
    return value;
}

static void cf8_write8(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint8_t value)
{
    bool intr_state = spinlock_acquire_intr(&cf8_lock);
    port_write32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    port_write8(PCI_CFC + (offset & 3), value);
    spinlock_release_intr(&cf8_lock, intr_state);
}

static void cf8_write16(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint16_t value)
{
    bool intr_state = spinlock_acquire_intr(&cf8_lock);
    port_write32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    port_write16(PCI_CFC + (offset & 2), value);
    spinlock_release_intr(&cf8_lock, intr_state);
}

static void cf8_write32(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint32_t value)
{
    bool intr_state = spinlock_acquire_intr(&cf8_lock);
    port_write32(PCI_CF8, cf8_addr(bus, device_index, function, offset));
    port_write32(PCI_CFC, value);
    spinlock_release_intr(&cf8_lock, intr_state);
}

static const pci_bus_ops_t cf8_ops = {
    .read8 = &cf8_read8,
    .read16 = &cf8_read16,
    .read32 = &cf8_read32,
    .write8 = &cf8_write8,
    .write16 = &cf8_write16,
    .write32 = &cf8_write32,
};

static pci_bus_t cf8_bus = {
    .ops = &cf8_ops,
    .name = "CF8",
    .segment = 0,
    .index = 0,
};

static void arch_init_pci_CF8(void)
{
    pci_bus_add(&cf8_bus);
}

initcall_bsp_bus(arch_init_pci_CF8, INIT_FIRST);
