// SPDX-License-Identifier: BSD-3-Clause

#include <bus/pci.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <mm/slab.h>

#define PCI_MAX_BUS_DEVICES 32

static struct pci_bus* bus_head = nullptr;
static struct pci_driver* driver_head = nullptr;

static void try_bind_device(struct pci_device* restrict device, struct pci_driver* restrict driver)
{
    const struct pci_driver_id* id;

    if(device->driver) {
        return; // already bound to a driver
    }

    for(id = driver->ids; id->vendor_id < 0xFFFF || id->device_id < 0xFFFF; id += 1) {
        if(id->vendor_id == device->vendor_id && id->device_id == device->device_id) {
            if(0 == driver->ops->probe(device, id)) {
                device->driver = driver;
            }
        }
    }
}

static void rescan_bus(struct pci_bus* restrict bus)
{
    unsigned device_idx;
    unsigned function_idx, function_max;
    unsigned vendor_id, device_id;
    unsigned header_value;
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
            device = slab_alloc(sizeof(struct pci_device));

            if(device == nullptr) {
                continue; // allocation failed
            }

            device->driver = nullptr;
            device->bus = bus;
            device->index = device_idx;
            device->function = function_idx;
            device->vendor_id = vendor_id;
            device->device_id = device_id;
            device->privdata = nullptr;

            device->next = bus->devices;
            bus->devices = device;

            kprintf(KP_INFORM, "pci: found %04x:%04x at %s:%u:%u", vendor_id, device_id, bus->identifier, device_idx, function_idx);

            for(driver = driver_head; driver; driver = driver->next) {
                try_bind_device(device, driver);
            }
        }
    }
}

int register_pci_bus(struct pci_bus* restrict bus)
{
    struct pci_bus* it;

    kassert(bus && "invalid PCI bus pointer");
    kassert(bus->ops && "invalid PCI bus ops pointer");

    for(it = bus_head; it; it = it->next) {
        if(it == bus)
            return EEXIST;
        continue;
    }

    kprintf(KP_INFORM, "pci: adding %s: segment=%u, bus_index=%u", bus->identifier, bus->segment, bus->index);

    bus->devices = nullptr;
    bus->next = bus_head;
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

    kassert(bus && "invalid pci_bus pointer");

    if(bus_head == bus) {
        bus_head = bus->next;
    }
    else {
        for(it = bus_head; it; it = it->next) {
            if(it == bus) {
                prev->next = it->next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENODEV;
        }
    }

    for(device = bus->devices; device; device = next) {
        next = device->next;

        if(device->driver && device->driver->ops->remove) {
            device->driver->ops->remove(device);
        }

        slab_free(device);
    }

    kprintf(KP_INFORM, "pci: removing %s", bus->identifier);

    return 0;
}

int register_pci_driver(struct pci_driver* restrict driver)
{
    struct pci_driver* it;
    struct pci_bus* bus;
    struct pci_device* device;

    kassert(driver && "invalid PCI driver pointer");
    kassert(driver->ops && "invalid PCI driver ops pointer");
    kassert(driver->ids && "invalid PCI driver ids pointer");

    for(it = driver_head; it; it = it->next) {
        if(it == driver)
            return EEXIST;
        continue;
    }

    driver->next = driver_head;
    driver_head = driver;

    for(bus = bus_head; bus; bus = bus->next) {
        for(device = bus->devices; device; device = device->next) {
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

    kassert(driver && "invalid PCI driver pointer");

    if(driver_head == driver) {
        driver_head = driver->next;
    }
    else {
        for(it = driver_head; it; it = it->next) {
            if(it == driver) {
                prev->next = it->next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENODEV;
        }
    }

    if(driver->ops->remove) {
        for(bus = bus_head; bus; bus = bus->next) {
            for(device = bus->devices; device; device = device->next) {
                if(device->driver == driver) {
                    driver->ops->remove(device);
                    device->driver = nullptr;
                }
            }
        }
    }

    return 0;
}

struct pci_device* pci_find(unsigned vendor_id, unsigned device_id, unsigned index)
{
    struct pci_bus* bus;
    struct pci_device* device;
    unsigned found = 0;

    for(bus = bus_head; bus; bus = bus->next) {
        for(device = bus->devices; device; device = device->next) {
            if(device->vendor_id == vendor_id && device->device_id == device_id) {
                if(found == index)
                    return device;
                found += 1;
            }
        }
    }

    return nullptr;
}

struct pci_device* pci_find_raw(unsigned segment, unsigned bus, unsigned device, unsigned function)
{
    struct pci_bus* it;
    struct pci_device* dev;

    for(it = bus_head; it; it = it->next) {
        if(it->segment == segment && it->index == bus) {
            for(dev = it->devices; dev; dev = dev->next) {
                if(dev->index == device && dev->function == function) {
                    return dev;
                }
            }
        }
    }

    return nullptr;
}

uint8_t pci_read8(struct pci_device* restrict device, uintptr_t offset)
{
    kassert(device && "invalid PCI device pointer");

    return device->bus->ops->read8(device->bus, device->index, device->function, offset);
}

uint16_t pci_read16(struct pci_device* restrict device, uintptr_t offset)
{
    kassert(device && "invalid PCI device pointer");

    return device->bus->ops->read16(device->bus, device->index, device->function, offset);
}

uint32_t pci_read32(struct pci_device* restrict device, uintptr_t offset)
{
    kassert(device && "invalid PCI device pointer");

    return device->bus->ops->read32(device->bus, device->index, device->function, offset);
}

void pci_write8(struct pci_device* restrict device, uintptr_t offset, uint8_t value)
{
    kassert(device && "invalid PCI device pointer");

    device->bus->ops->write8(device->bus, device->index, device->function, offset, value);
}

void pci_write16(struct pci_device* restrict device, uintptr_t offset, uint16_t value)
{
    kassert(device && "invalid PCI device pointer");

    device->bus->ops->write16(device->bus, device->index, device->function, offset, value);
}

void pci_write32(struct pci_device* restrict device, uintptr_t offset, uint32_t value)
{
    kassert(device && "invalid PCI device pointer");

    device->bus->ops->write32(device->bus, device->index, device->function, offset, value);
}
