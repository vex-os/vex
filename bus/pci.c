#include <arch/ireq.h>
#include <bus/pci.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <ireq/intctl.h>
#include <limits.h>
#include <mm/hhdm.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/vmm.h>
#include <mm/vprot.h>

#define PCI_MAX_BUS_DEVICES 32

static struct pci_bus* bus_head = nullptr;
static struct pci_driver* driver_head = nullptr;

static void try_bind_device(struct pci_device* restrict device, struct pci_driver* restrict driver)
{
    const struct pci_driver_id* id;

    if(device->priv.driver) {
        return; // already bound to a driver
    }

    for(id = driver->ids; id->vendor_id < 0xFFFF || id->device_id < 0xFFFF; id += 1) {
        bool match;

        if(id->vendor_id == 0) {
            match = id->class_id == device->class_id && id->subclass_id == device->subclass_id;
        }
        else {
            match = id->vendor_id == device->vendor_id && id->device_id == device->device_id;
        }

        if(match) {
            if(0 == driver->ops->probe(device, id)) {
                device->priv.driver = driver;
            }
            return;
        }
    }
}

static void rescan_bus(struct pci_bus* restrict bus)
{
    unsigned device_idx;
    unsigned function_idx, function_max;
    unsigned vendor_id, device_id;
    unsigned header_value;
    unsigned class_id, subclass_id;
    struct pci_device* device;
    struct pci_driver* driver;

    for(device_idx = 0; device_idx < PCI_MAX_BUS_DEVICES; device_idx += 1) {
        vendor_id = bus->ops->read16(bus, device_idx, 0, PCI_CFG_VENDOR_ID);

        if(vendor_id == 0xFFFF) {
            continue; // no device present
        }

        header_value = bus->ops->read8(bus, device_idx, 0, PCI_CFG_HEADER_TYPE);

        if(header_value & PCI_HDR_MULTIFUNC) {
            function_max = 8;
        }
        else {
            function_max = 1;
        }

        for(function_idx = 0; function_idx < function_max; function_idx += 1) {
            vendor_id = bus->ops->read16(bus, device_idx, function_idx, PCI_CFG_VENDOR_ID);

            if(vendor_id == 0xFFFF) {
                continue; // no device present
            }

            device_id = bus->ops->read16(bus, device_idx, function_idx, PCI_CFG_DEVICE_ID);
            device = kmalloc(sizeof(struct pci_device));

            if(device == nullptr) {
                continue; // allocation failed
            }

            class_id = bus->ops->read8(bus, device_idx, function_idx, PCI_CFG_CLASS);
            subclass_id = bus->ops->read8(bus, device_idx, function_idx, PCI_CFG_SUBCLASS);

            device->priv.next = nullptr;
            device->priv.driver = nullptr;
            device->priv.ireq_number = UINT_MAX;
            device->priv.bars_mapped = 0;
            device->bus = bus;
            device->index = device_idx;
            device->function = function_idx;
            device->vendor_id = vendor_id;
            device->device_id = device_id;
            device->class_id = class_id;
            device->subclass_id = subclass_id;
            device->ctxdata = nullptr;

            device->priv.next = bus->priv.devices;
            bus->priv.devices = device;

            pr_debug("pci: found %04x:%04x at %s:%u:%u", vendor_id, device_id, bus->identifier, device_idx, function_idx);

            for(driver = driver_head; driver; driver = driver->priv.next) {
                try_bind_device(device, driver);
            }
        }
    }
}

static uintptr_t get_bar_address(struct pci_device* restrict device, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_cfg_read32(device, offset);
    uint64_t raw_higher;

    if(raw & PCI_BAR_IO) {
        return (uintptr_t)(raw & (uint32_t)(PCI_BAR_ADDR_MASK_IO));
    }

    if((raw & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_64BIT) {
        raw_higher = pci_cfg_read32(device, offset + 4);

        return (uintptr_t)((raw_higher << 32) | (uint64_t)(raw & (uint32_t)(PCI_BAR_ADDR_MASK_MEM)));
    }

    return (uintptr_t)(raw & (uint32_t)(PCI_BAR_ADDR_MASK_MEM));
}

static size_t get_bar_length(struct pci_device* restrict device, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_cfg_read32(device, offset);
    uint32_t mask;

    pci_cfg_write32(device, offset, UINT32_C(0xFFFFFFFF));
    mask = pci_cfg_read32(device, offset);
    pci_cfg_write32(device, offset, raw);

    if(raw & PCI_BAR_IO) {
        mask &= (uint32_t)(PCI_BAR_ADDR_MASK_IO);
    }
    else {
        mask &= (uint32_t)(PCI_BAR_ADDR_MASK_MEM);
    }

    return (size_t)(~mask) + 1;
}

static void* bar_map_mmio(struct pci_device* restrict device, unsigned bar_index)
{
    uintptr_t phys, i;
    size_t size;

    phys = get_bar_address(device, bar_index);
    size = get_bar_length(device, bar_index);

    for(i = 0; i < size; i += PAGE_SIZE) {
        vmm_map(&sys_pagemap, (uintptr_t)(phys_to_virt(phys + i)), phys + i, VPROT_RW | VPROT_NOCACHE);
    }

    return phys_to_virt(phys);
}

static void bar_unmap_mmio(struct pci_device* restrict device, unsigned bar_index)
{
    uintptr_t phys, i;
    size_t size;

    phys = get_bar_address(device, bar_index);
    size = get_bar_length(device, bar_index);

    for(i = 0; i < size; i += PAGE_SIZE) {
        vmm_unmap(&sys_pagemap, (uintptr_t)(phys_to_virt(phys + i)));
    }
}

int register_pci_bus(struct pci_bus* restrict bus)
{
    struct pci_bus* it;

    for(it = bus_head; it; it = it->priv.next) {
        if(it == bus) {
            return EEXIST;
        }
    }

    pr_debug("pci: adding %s: segment=%u, bus_index=%u", bus->identifier, bus->segment, bus->index);

    bus->priv.devices = nullptr;
    bus->priv.next = bus_head;
    bus_head = bus;

    rescan_bus(bus);

    return 0;
}

int unregister_pci_bus(struct pci_bus* restrict bus)
{
    struct pci_bus* it;
    struct pci_bus* prev = nullptr;
    struct pci_device* device;
    struct pci_device* next;

    if(bus_head == bus) {
        bus_head = bus->priv.next;
    }
    else {
        for(it = bus_head; it; it = it->priv.next) {
            if(it == bus) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENODEV;
        }
    }

    for(device = bus->priv.devices; device; device = next) {
        next = device->priv.next;

        if(device->priv.driver && device->priv.driver->ops->remove) {
            device->priv.driver->ops->remove(device);
        }

        kfree(device);
    }

    pr_debug("pci: removing %s", bus->identifier);

    return 0;
}

int register_pci_driver(struct pci_driver* restrict driver)
{
    struct pci_driver* it;
    struct pci_bus* bus;
    struct pci_device* device;

    for(it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            return EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(bus = bus_head; bus; bus = bus->priv.next) {
        for(device = bus->priv.devices; device; device = device->priv.next) {
            try_bind_device(device, driver);
        }
    }

    return 0;
}

int unregister_pci_driver(struct pci_driver* restrict driver)
{
    struct pci_driver* it;
    struct pci_driver* prev = nullptr;
    struct pci_bus* bus;
    struct pci_device* device;

    if(driver_head == driver) {
        driver_head = driver->priv.next;
    }
    else {
        for(it = driver_head; it; it = it->priv.next) {
            if(it == driver) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENODEV;
        }
    }

    if(driver->ops->remove) {
        for(bus = bus_head; bus; bus = bus->priv.next) {
            for(device = bus->priv.devices; device; device = device->priv.next) {
                if(device->priv.driver == driver) {
                    driver->ops->remove(device);
                    device->priv.driver = nullptr;
                }
            }
        }
    }

    return 0;
}

struct pci_device* pci_find(unsigned segment, unsigned bus, unsigned device, unsigned function)
{
    struct pci_bus* it;
    struct pci_device* dev;

    for(it = bus_head; it; it = it->priv.next) {
        if(it->segment == segment && it->index == bus) {
            for(dev = it->priv.devices; dev; dev = dev->priv.next) {
                if(dev->index == device && dev->function == function) {
                    return dev;
                }
            }
        }
    }

    return nullptr;
}

uint8_t pci_cfg_read8(struct pci_device* restrict device, uintptr_t offset)
{
    return device->bus->ops->read8(device->bus, device->index, device->function, offset);
}

uint16_t pci_cfg_read16(struct pci_device* restrict device, uintptr_t offset)
{
    return device->bus->ops->read16(device->bus, device->index, device->function, offset);
}

uint32_t pci_cfg_read32(struct pci_device* restrict device, uintptr_t offset)
{
    return device->bus->ops->read32(device->bus, device->index, device->function, offset);
}

void pci_cfg_write8(struct pci_device* restrict device, uintptr_t offset, uint8_t value)
{
    device->bus->ops->write8(device->bus, device->index, device->function, offset, value);
}

void pci_cfg_write16(struct pci_device* restrict device, uintptr_t offset, uint16_t value)
{
    device->bus->ops->write16(device->bus, device->index, device->function, offset, value);
}

void pci_cfg_write32(struct pci_device* restrict device, uintptr_t offset, uint32_t value)
{
    device->bus->ops->write32(device->bus, device->index, device->function, offset, value);
}

struct pci_bar* pci_bar_map(struct pci_device* restrict device, unsigned bar_index)
{
    struct pci_bar* bar;
    size_t size;
    uint32_t raw;
    void* virt;

    weak_kassert(bar_index < 6);

    if(device->priv.bars_mapped & (1U << bar_index)) {
        return &device->priv.bars[bar_index];
    }

    bar = &device->priv.bars[bar_index];
    raw = pci_cfg_read32(device, PCI_CFG_BAR0 + bar_index * 4);

    if(raw & PCI_BAR_IO) {
        bar->type = PCI_BAR_TYPE_PMIO;
        bar->pmio.base = raw & (uint32_t)(PCI_BAR_ADDR_MASK_IO);
        bar->pmio.length = get_bar_length(device, bar_index);
        bar->pmio_ctxdata = nullptr;
    }
    else {
        bar->type = PCI_BAR_TYPE_MMIO;
        size = get_bar_length(device, bar_index);
        virt = bar_map_mmio(device, bar_index);
        bar->mmio.virt8 = (volatile uint8_t*)(virt);
        bar->mmio.virt16 = (volatile uint16_t*)(virt);
        bar->mmio.virt32 = (volatile uint32_t*)(virt);
        bar->mmio.virt64 = (volatile uint64_t*)(virt);
        bar->mmio.length = size;
    }

    device->priv.bars_mapped |= (1U << bar_index);

    return bar;
}

void pci_bar_unmap(struct pci_device* restrict device, unsigned bar_index)
{
    struct pci_bar* bar;

    weak_kassert(bar_index < 6);

    if(device->priv.bars_mapped && (1U << bar_index)) {
        bar = &device->priv.bars[bar_index];

        if(bar->type == PCI_BAR_TYPE_MMIO) {
            bar_unmap_mmio(device, bar_index);
        }

        device->priv.bars_mapped &= ~(1U << bar_index);
    }
}

uint8_t pci_bar_read8(struct pci_bar* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return arch_pmio_read8(&bar->pmio, bar->pmio_ctxdata, offset);
    return bar->mmio.virt8[offset];
}

uint16_t pci_bar_read16(struct pci_bar* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return arch_pmio_read16(&bar->pmio, bar->pmio_ctxdata, offset);
    return bar->mmio.virt16[offset / 2];
}

uint32_t pci_bar_read32(struct pci_bar* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return arch_pmio_read32(&bar->pmio, bar->pmio_ctxdata, offset);
    return bar->mmio.virt32[offset / 4];
}

uint64_t pci_bar_read64(struct pci_bar* restrict bar, uintptr_t offset)
{
    return bar->mmio.virt64[offset / 8];
}

void pci_bar_write8(struct pci_bar* restrict bar, uintptr_t offset, uint8_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        arch_pmio_write8(&bar->pmio, bar->pmio_ctxdata, offset, value);
    }
    else {
        bar->mmio.virt8[offset] = value;
    }
}

void pci_bar_write16(struct pci_bar* restrict bar, uintptr_t offset, uint16_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        arch_pmio_write16(&bar->pmio, bar->pmio_ctxdata, offset, value);
    }
    else {
        bar->mmio.virt16[offset / 2] = value;
    }
}

void pci_bar_write32(struct pci_bar* restrict bar, uintptr_t offset, uint32_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        arch_pmio_write32(&bar->pmio, bar->pmio_ctxdata, offset, value);
    }
    else {
        bar->mmio.virt32[offset / 4] = value;
    }
}

void pci_bar_write64(struct pci_bar* restrict bar, uintptr_t offset, uint64_t value)
{
    weak_kassert(bar->type == PCI_BAR_TYPE_MMIO);

    bar->mmio.virt64[offset / 8] = value;
}

uint8_t pci_get_cap(struct pci_device* restrict device, uint8_t cap_id)
{
    uint16_t status;
    uint8_t offset;

    status = pci_cfg_read16(device, PCI_CFG_STATUS);

    if(0 == (status & PCI_STS_CAP_LIST)) {
        return 0;
    }

    offset = pci_cfg_read8(device, PCI_CFG_CAP_PTR) & 0xFC;

    while(offset) {
        if(pci_cfg_read8(device, offset) == cap_id)
            return offset;
        offset = pci_cfg_read8(device, offset + 1) & 0xFC;
    }

    return 0;
}

void pci_enable_busmaster(struct pci_device* restrict device)
{
    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);
    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd | PCI_CMD_BUS_MASTER);
}

void pci_disable_busmaster(struct pci_device* restrict device)
{
    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);
    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd & ~(uint16_t)(PCI_CMD_BUS_MASTER));
}

int pci_request_irq(struct pci_device* restrict device, ireq_handler_type handler, void* restrict ctxdata)
{
    struct intctl* controller;
    uint8_t int_pin, int_line;
    unsigned ireq_number;

    int_pin = pci_cfg_read8(device, PCI_CFG_INT_PIN);

    if(int_pin == 0) {
        return ENODEV; // device does not use INTx
    }

    int_line = pci_cfg_read8(device, PCI_CFG_INT_LINE);

    if(int_line == 0xFF) {
        return ENODEV; // not routed by BIOS
    }

    controller = find_intctl(int_line);

    if(controller == nullptr) {
        return ENODEV;
    }

    ireq_number = register_ireq_handler(UINT_MAX, handler, ctxdata);

    if(ireq_number == UINT_MAX) {
        return ENOMEM;
    }

    device->priv.ireq_number = ireq_number;

    // PCI INTx is always level-triggered, active-low per PCI spec
    intctl_route(controller, int_line, (uint8_t)(ireq_number + ARCH_IREQ_BASE), 0, TRIGGER_LEVEL, ACTIVE_LOW);
    intctl_unmask(controller, int_line);

    return 0;
}

void pci_release_irq(struct pci_device* restrict device)
{
    struct intctl* controller;
    uint8_t int_line;

    if(device->priv.ireq_number == UINT_MAX) {
        return;
    }

    int_line = pci_cfg_read8(device, PCI_CFG_INT_LINE);
    controller = find_intctl(int_line);

    if(controller) {
        intctl_mask(controller, int_line);
    }

    unregister_ireq_handler(device->priv.ireq_number);

    device->priv.ireq_number = UINT_MAX;
}
