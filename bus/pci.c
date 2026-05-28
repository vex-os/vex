// SPDX-License-Identifier: BSD-3-Clause

#include <bus/pci.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <mm/slab.h>

static struct pci_bus* bus_head = nullptr;
static struct pci_device* device_head = nullptr;
static struct pci_driver* driver_head = nullptr;

static struct pci_bus* find_bus(uint16_t segment, uint8_t bus_index)
{
    struct pci_bus* it;

    for(it = bus_head; it; it = it->next) {
        if(it->segment == segment && it->bus_index == bus_index) {
            return it;
        }
    }

    return nullptr;
}

static void try_bind(struct pci_device* restrict device, struct pci_driver* restrict driver)
{
    size_t i;

    if(device->driver) {
        return;
    }

    for(i = 0; i < driver->num_ids; ++i) {
        if(driver->id_table[i].vendor != device->vendor_id) {
            continue;
        }

        if(driver->id_table[i].device != device->device_id) {
            continue;
        }

        if(0 == driver->ops->probe(device, &driver->id_table[i])) {
            device->driver = driver;

            kprintf(KP_INFORM, "pci: %04x:%04x at %s:%u:%u bound to driver", (unsigned)(device->vendor_id), (unsigned)(device->device_id),
                device->bus->identity, (unsigned)(device->device_index), (unsigned)(device->function));
        }

        return;
    }
}

static void scan_bus(struct pci_bus* restrict bus)
{
    struct pci_device* device;
    struct pci_driver* driver;
    uint8_t dev, func, header, max_func;
    uint16_t vendor, device_id;

    for(dev = 0; dev < 32; ++dev) {
        vendor = bus->ops->read16(bus, dev, 0, PCI_CFG_VENDOR_ID);

        if(vendor == 0xFFFF) {
            continue;
        }

        header = bus->ops->read8(bus, dev, 0, PCI_CFG_HEADER_TYPE);

        if(header & PCI_HDR_MULTIFUNC) {
            max_func = 8;
        }
        else {
            max_func = 1;
        }

        for(func = 0; func < max_func; ++func) {
            vendor = bus->ops->read16(bus, dev, func, PCI_CFG_VENDOR_ID);

            if(vendor == 0xFFFF) {
                continue;
            }

            device_id = bus->ops->read16(bus, dev, func, PCI_CFG_DEVICE_ID);
            device = slab_alloc(sizeof(struct pci_device));

            if(device == nullptr) {
                continue;
            }

            device->next = device_head;
            device->bus = bus;
            device->driver = nullptr;
            device->device_index = dev;
            device->function = func;
            device->vendor_id = vendor;
            device->device_id = device_id;
            device->privdata = nullptr;

            device_head = device;

            kprintf(KP_INFORM, "pci: found %04x:%04x at %s:%u:%u", (unsigned)(vendor), (unsigned)(device_id), bus->identity,
                (unsigned)(dev), (unsigned)(func));

            for(driver = driver_head; driver; driver = driver->next) {
                try_bind(device, driver);
            }
        }
    }
}

int register_pci_bus(struct pci_bus* restrict bus)
{
    struct pci_bus* it;

    kassert(bus && "invalid pci_bus pointer");
    kassert(bus->ops && "invalid pci_bus ops pointer");

    for(it = bus_head; it; it = it->next) {
        if(it == bus)
            return EEXIST;
        continue;
    }

    kprintf(KP_INFORM, "pci: adding %s: segment=%zu, bus_index=%zu", bus->identity, (size_t)(bus->segment), (size_t)(bus->bus_index));

    bus->next = bus_head;
    bus_head = bus;

    scan_bus(bus);

    return 0;
}

int register_pci_driver(struct pci_driver* restrict driver)
{
    struct pci_driver* it;
    struct pci_device* device;

    kassert(driver && "invalid pci_driver pointer");
    kassert(driver->ops && "invalid pci_driver ops pointer");
    kassert(driver->id_table && "invalid pci_driver id_table pointer");

    for(it = driver_head; it; it = it->next) {
        if(it == driver)
            return EEXIST;
        continue;
    }

    driver->next = driver_head;
    driver_head = driver;

    for(device = device_head; device; device = device->next) {
        try_bind(device, driver);
    }

    return 0;
}

struct pci_device* pci_open(uint16_t segment, uint8_t bus_index, uint8_t device_index, uint8_t function)
{
    struct pci_bus* bus = find_bus(segment, bus_index);
    struct pci_device* device;

    if(bus == nullptr) {
        return nullptr;
    }

    device = slab_alloc(sizeof(struct pci_device));

    if(device == nullptr) {
        return nullptr;
    }

    device->next = nullptr;
    device->bus = bus;
    device->driver = nullptr;
    device->device_index = device_index;
    device->function = function;
    device->vendor_id = bus->ops->read16(bus, device_index, function, PCI_CFG_VENDOR_ID);
    device->device_id = bus->ops->read16(bus, device_index, function, PCI_CFG_DEVICE_ID);
    device->privdata = nullptr;

    return device;
}

void pci_close(struct pci_device* restrict device)
{
    kassert(device && "invalid pci device pointer");

    slab_free(device);
}

uint8_t pci_read8(struct pci_device* restrict device, uint16_t offset)
{
    kassert(device && "invalid pci device pointer");

    return device->bus->ops->read8(device->bus, device->device_index, device->function, offset);
}

uint16_t pci_read16(struct pci_device* restrict device, uint16_t offset)
{
    kassert(device && "invalid pci device pointer");

    return device->bus->ops->read16(device->bus, device->device_index, device->function, offset);
}

uint32_t pci_read32(struct pci_device* restrict device, uint16_t offset)
{
    kassert(device && "invalid pci device pointer");

    return device->bus->ops->read32(device->bus, device->device_index, device->function, offset);
}

void pci_write8(struct pci_device* restrict device, uint16_t offset, uint8_t value)
{
    kassert(device && "invalid pci device pointer");

    device->bus->ops->write8(device->bus, device->device_index, device->function, offset, value);
}

void pci_write16(struct pci_device* restrict device, uint16_t offset, uint16_t value)
{
    kassert(device && "invalid pci device pointer");

    device->bus->ops->write16(device->bus, device->device_index, device->function, offset, value);
}

void pci_write32(struct pci_device* restrict device, uint16_t offset, uint32_t value)
{
    kassert(device && "invalid pci device pointer");

    device->bus->ops->write32(device->bus, device->device_index, device->function, offset, value);
}
