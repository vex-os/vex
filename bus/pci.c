// SPDX-License-Identifier: BSD-3-Clause

#include <bus/pci.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <mm/slab.h>

static struct pci_bus* bus_head = nullptr;

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

    return 0;
}

struct pci_device* pci_open(uint16_t segment, uint8_t bus_index, uint8_t device_index, uint8_t function)
{
    struct pci_bus* bus = find_bus(segment, bus_index);

    if(bus == nullptr) {
        return nullptr;
    }

    struct pci_device* device = slab_alloc(sizeof(struct pci_device));

    if(device == nullptr) {
        return nullptr;
    }

    device->bus = bus;
    device->device_index = device_index;
    device->function = function;

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
