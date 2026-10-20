#include <arch/irq.h>
#include <arch/msi.h>
#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/mmio.h>
#include <vex/mutex.h>
#include <vex/page.h>
#include <vex/pci.h>
#include <vex/vprot.h>

#define PCI_MAX_BUS_DEVICES 32

static mutex_t pci_mutex = MUTEX_INIT;
static pci_bus_t* bus_head = nullptr;
static pci_driver_t* driver_head = nullptr;

static void device_bind(pci_device_t* restrict device, pci_driver_t* restrict driver)
{
    if(device->priv.driver) {
        return;
    }

    for(const pci_driver_id_t* id = driver->ids; id->vendor_id < 0xFFFF || id->device_id < 0xFFFF; id += 1) {
        bool match;

        if(id->vendor_id == 0) {
            match = id->class_id == device->class_id && id->subclass_id == device->subclass_id;
        }
        else {
            match = id->vendor_id == device->vendor_id && id->device_id == device->device_id;
        }

        if(match) {
            if(0 == driver->ops->probe(device, id))
                device->priv.driver = driver;
            return;
        }
    }
}

static void bus_scan(pci_bus_t* restrict bus)
{
    for(unsigned dev = 0; dev < PCI_MAX_BUS_DEVICES; dev += 1) {
        unsigned vendor_id = bus->ops->read16(bus, dev, 0, PCI_CFG_VENDOR_ID);

        if(vendor_id == 0xFFFF) {
            continue;
        }

        unsigned header_value = bus->ops->read8(bus, dev, 0, PCI_CFG_HEADER_TYPE);
        unsigned function_max;

        if(header_value & PCI_HDR_MULTIFUNC) {
            function_max = 8;
        }
        else {
            function_max = 1;
        }

        for(unsigned fn = 0; fn < function_max; fn += 1) {
            vendor_id = bus->ops->read16(bus, dev, fn, PCI_CFG_VENDOR_ID);

            if(vendor_id == 0xFFFF) {
                continue;
            }

            unsigned device_id = bus->ops->read16(bus, dev, fn, PCI_CFG_DEVICE_ID);
            pci_device_t* device = kmalloc(sizeof(pci_device_t));

            if(device == nullptr) {
                continue;
            }

            unsigned class_id = bus->ops->read8(bus, dev, fn, PCI_CFG_CLASS);
            unsigned subclass_id = bus->ops->read8(bus, dev, fn, PCI_CFG_SUBCLASS);

            device->priv.next = nullptr;
            device->priv.driver = nullptr;
            device->priv.irq_number = UINT_MAX;
            device->priv.bars_mapped = 0;
            device->bus = bus;
            device->index = dev;
            device->function = fn;
            device->vendor_id = vendor_id;
            device->device_id = device_id;
            device->class_id = class_id;
            device->subclass_id = subclass_id;
            device->ctxdata = nullptr;

            device->priv.next = bus->priv.devices;
            bus->priv.devices = device;

            pr_verbose("pci: found %04X:%04X at %s:%02X:%02X", vendor_id, device_id, bus->name, dev, fn);

            for(pci_driver_t* it = driver_head; it; it = it->priv.next) {
                device_bind(device, it);
            }
        }
    }
}

static uintptr_t bar_address(pci_device_t* restrict device, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_cfg_read32(device, offset);

    if(raw & PCI_BAR_IO) {
        return raw & PCI_BAR_ADDR_MASK_IO;
    }

    if((raw & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_64BIT) {
        uint64_t raw_higher = pci_cfg_read32(device, offset + 4);

        return (raw & PCI_BAR_ADDR_MASK_MEM) | (raw_higher << 32);
    }

    return raw & PCI_BAR_ADDR_MASK_MEM;
}

static uint32_t probe_bar_mask(pci_device_t* restrict device, uintptr_t offset)
{
    uint32_t raw = pci_cfg_read32(device, offset);
    pci_cfg_write32(device, offset, UINT32_C(0xFFFFFFFF));
    uint32_t mask = pci_cfg_read32(device, offset);
    pci_cfg_write32(device, offset, raw);
    return mask;
}

static uint64_t bar_length(pci_device_t* restrict device, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_cfg_read32(device, offset);

    // A half-written BAR would decode at
    // a bogus address while it is being probed
    // so we disable the BAR while probing its length
    uint16_t command = pci_cfg_read16(device, PCI_CFG_COMMAND);

    pci_cfg_write16(device, PCI_CFG_COMMAND, command & ~(PCI_COMMAND_IO | PCI_COMMAND_MEM));

    uint64_t mask = probe_bar_mask(device, offset);

    if(0 == (raw & PCI_BAR_IO) && (raw & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_64BIT && bar_index < 5) {
        mask |= (uint64_t)(probe_bar_mask(device, offset + 4)) << 32;
    }
    else {
        mask |= UINT64_C(0xFFFFFFFF00000000);
    }

    pci_cfg_write16(device, PCI_CFG_COMMAND, command);

    if(raw & PCI_BAR_IO) {
        // I/O BARs may hardwire the upper 16 bits to zero
        mask = (mask & PCI_BAR_ADDR_MASK_IO) | UINT64_C(0xFFFFFFFFFFFF0000);
    }
    else {
        mask &= PCI_BAR_ADDR_MASK_MEM;
    }

    if(mask == UINT64_C(0xFFFFFFFF00000000) || mask == UINT64_C(0xFFFFFFFFFFFF0000)) {
        return 0;
    }

    return ~mask + 1;
}

int pci_bus_add(pci_bus_t* restrict bus)
{
    mutex_acquire(&pci_mutex);

    for(pci_bus_t* it = bus_head; it; it = it->priv.next) {
        if(it == bus) {
            mutex_release(&pci_mutex);
            return -EEXIST;
        }
    }

    bus->priv.devices = nullptr;
    bus->priv.next = bus_head;
    bus_head = bus;

    bus_scan(bus);

    mutex_release(&pci_mutex);

    return 0;
}

int pci_bus_remove(pci_bus_t* restrict bus)
{
    mutex_acquire(&pci_mutex);

    pci_bus_t** prev = &bus_head;

    while(*prev && *prev != bus) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&pci_mutex);
        return -ENODEV;
    }

    *prev = bus->priv.next;

    for(pci_device_t* it = bus->priv.devices; it;) {
        pci_device_t* next = it->priv.next;

        if(it->priv.driver && it->priv.driver->ops->remove) {
            it->priv.driver->ops->remove(it);
        }

        kfree(it);

        it = next;
    }

    bus->priv.devices = nullptr;

    mutex_release(&pci_mutex);

    return 0;
}

int pci_driver_add(pci_driver_t* restrict driver)
{
    mutex_acquire(&pci_mutex);

    for(pci_driver_t* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            mutex_release(&pci_mutex);
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(pci_bus_t* bus = bus_head; bus; bus = bus->priv.next) {
        for(pci_device_t* it = bus->priv.devices; it; it = it->priv.next) {
            device_bind(it, driver);
        }
    }

    mutex_release(&pci_mutex);

    return 0;
}

int pci_driver_remove(pci_driver_t* restrict driver)
{
    mutex_acquire(&pci_mutex);

    pci_driver_t** prev = &driver_head;

    while(*prev && *prev != driver) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&pci_mutex);
        return -ENODEV;
    }

    *prev = driver->priv.next;

    for(pci_bus_t* bus = bus_head; bus; bus = bus->priv.next) {
        for(pci_device_t* it = bus->priv.devices; it; it = it->priv.next) {
            if(it->priv.driver == driver) {
                if(driver->ops->remove) {
                    driver->ops->remove(it);
                }

                it->priv.driver = nullptr;
            }
        }
    }

    mutex_release(&pci_mutex);

    return 0;
}

pci_device_t* pci_find(unsigned segment, unsigned bus, unsigned device, unsigned function)
{
    mutex_acquire(&pci_mutex);

    for(pci_bus_t* it = bus_head; it; it = it->priv.next) {
        if(it->segment == segment && it->index == bus) {
            for(pci_device_t* dev = it->priv.devices; dev; dev = dev->priv.next) {
                if(dev->index == device && dev->function == function) {
                    mutex_release(&pci_mutex);
                    return dev;
                }
            }
        }
    }

    mutex_release(&pci_mutex);

    return nullptr;
}

uint8_t pci_cfg_read8(pci_device_t* restrict device, uintptr_t offset)
{
    return device->bus->ops->read8(device->bus, device->index, device->function, offset);
}

uint16_t pci_cfg_read16(pci_device_t* restrict device, uintptr_t offset)
{
    return device->bus->ops->read16(device->bus, device->index, device->function, offset);
}

uint32_t pci_cfg_read32(pci_device_t* restrict device, uintptr_t offset)
{
    return device->bus->ops->read32(device->bus, device->index, device->function, offset);
}

void pci_cfg_write8(pci_device_t* restrict device, uintptr_t offset, uint8_t value)
{
    device->bus->ops->write8(device->bus, device->index, device->function, offset, value);
}

void pci_cfg_write16(pci_device_t* restrict device, uintptr_t offset, uint16_t value)
{
    device->bus->ops->write16(device->bus, device->index, device->function, offset, value);
}

void pci_cfg_write32(pci_device_t* restrict device, uintptr_t offset, uint32_t value)
{
    device->bus->ops->write32(device->bus, device->index, device->function, offset, value);
}

pci_bar_t* pci_bar_map(pci_device_t* restrict device, unsigned bar_index)
{
    kassert(bar_index < 6);

    if(device->priv.bars_mapped & (1U << bar_index)) {
        return &device->priv.bars[bar_index];
    }

    pci_bar_t* bar = &device->priv.bars[bar_index];
    uint32_t raw = pci_cfg_read32(device, PCI_CFG_BAR0 + bar_index * 4);
    uint64_t length = bar_length(device, bar_index);

    if(length == 0) {
        return nullptr;
    }

    if(raw & PCI_BAR_IO) {
        bar->type = PCI_BAR_TYPE_PMIO;
        bar->pmio.base = raw & PCI_BAR_ADDR_MASK_IO;
        bar->pmio.length = length;
        pmio_region_setup(&bar->pmio);
    }
    else {
        void* virt = mmio_map(bar_address(device, bar_index), length, VPROT_MMIO);

        if(virt == nullptr) {
            return nullptr;
        }

        bar->type = PCI_BAR_TYPE_MMIO;
        bar->mmio.virt8 = (volatile uint8_t*)(virt);
        bar->mmio.virt16 = (volatile uint16_t*)(virt);
        bar->mmio.virt32 = (volatile uint32_t*)(virt);
        bar->mmio.virt64 = (volatile uint64_t*)(virt);
        bar->mmio.length = length;
    }

    device->priv.bars_mapped |= (1U << bar_index);

    return bar;
}

void pci_bar_unmap(pci_device_t* restrict device, unsigned bar_index)
{
    kassert(bar_index < 6);

    if(device->priv.bars_mapped & (1U << bar_index)) {
        pci_bar_t* bar = &device->priv.bars[bar_index];

        if(bar->type == PCI_BAR_TYPE_MMIO) {
            mmio_unmap(bar->mmio.virt8);
        }

        device->priv.bars_mapped &= ~(1U << bar_index);
    }
}

uint8_t pci_bar_read8(pci_bar_t* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return pmio_read8(&bar->pmio, offset);
    return bar->mmio.virt8[offset];
}

uint16_t pci_bar_read16(pci_bar_t* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return pmio_read16(&bar->pmio, offset);
    return bar->mmio.virt16[offset / 2];
}

uint32_t pci_bar_read32(pci_bar_t* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return pmio_read32(&bar->pmio, offset);
    return bar->mmio.virt32[offset / 4];
}

uint64_t pci_bar_read64(pci_bar_t* restrict bar, uintptr_t offset)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);
    return bar->mmio.virt64[offset / 8];
}

void pci_bar_read8s(pci_bar_t* restrict bar, uintptr_t offset, uint8_t* restrict data, size_t count)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_read8s(&bar->pmio, offset, data, count);
    }
    else {
        for(size_t i = 0; i < count; ++i) {
            data[i] = bar->mmio.virt8[offset + i];
        }
    }
}

void pci_bar_read16s(pci_bar_t* restrict bar, uintptr_t offset, uint16_t* restrict data, size_t count)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_read16s(&bar->pmio, offset, data, count);
    }
    else {
        for(size_t i = 0; i < count; ++i) {
            data[i] = bar->mmio.virt16[(offset / 2) + i];
        }
    }
}

void pci_bar_read32s(pci_bar_t* restrict bar, uintptr_t offset, uint32_t* restrict data, size_t count)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_read32s(&bar->pmio, offset, data, count);
    }
    else {
        for(size_t i = 0; i < count; ++i) {
            data[i] = bar->mmio.virt32[(offset / 4) + i];
        }
    }
}

void pci_bar_read64s(pci_bar_t* restrict bar, uintptr_t offset, uint64_t* restrict data, size_t count)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);

    for(size_t i = 0; i < count; ++i) {
        data[i] = bar->mmio.virt64[(offset / 8) + i];
    }
}

void pci_bar_write8(pci_bar_t* restrict bar, uintptr_t offset, uint8_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write8(&bar->pmio, offset, value);
    }
    else {
        bar->mmio.virt8[offset] = value;
    }
}

void pci_bar_write16(pci_bar_t* restrict bar, uintptr_t offset, uint16_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write16(&bar->pmio, offset, value);
    }
    else {
        bar->mmio.virt16[offset / 2] = value;
    }
}

void pci_bar_write32(pci_bar_t* restrict bar, uintptr_t offset, uint32_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write32(&bar->pmio, offset, value);
    }
    else {
        bar->mmio.virt32[offset / 4] = value;
    }
}

void pci_bar_write64(pci_bar_t* restrict bar, uintptr_t offset, uint64_t value)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);

    bar->mmio.virt64[offset / 8] = value;
}

void pci_bar_write8s(pci_bar_t* restrict bar, uintptr_t offset, const uint8_t* restrict data, size_t count)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write8s(&bar->pmio, offset, data, count);
    }
    else {
        for(size_t i = 0; i < count; ++i) {
            bar->mmio.virt8[offset + i] = data[i];
        }
    }
}

void pci_bar_write16s(pci_bar_t* restrict bar, uintptr_t offset, const uint16_t* restrict data, size_t count)
{
    ;

    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write16s(&bar->pmio, offset, data, count);
    }
    else {
        for(size_t i = 0; i < count; ++i) {
            bar->mmio.virt16[(offset / 2) + i] = data[i];
        }
    }
}

void pci_bar_write32s(pci_bar_t* restrict bar, uintptr_t offset, const uint32_t* restrict data, size_t count)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write32s(&bar->pmio, offset, data, count);
    }
    else {
        for(size_t i = 0; i < count; ++i) {
            bar->mmio.virt32[(offset / 4) + i] = data[i];
        }
    }
}

void pci_bar_write64s(pci_bar_t* restrict bar, uintptr_t offset, const uint64_t* restrict data, size_t count)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);

    for(size_t i = 0; i < count; ++i) {
        bar->mmio.virt64[(offset / 8) + i] = data[i];
    }
}

uint8_t pci_get_cap(pci_device_t* restrict device, uint8_t cap_id)
{
    uint16_t status = pci_cfg_read16(device, PCI_CFG_STATUS);

    if(0 == (status & PCI_STS_CAP_LIST)) {
        return 0;
    }

    uint8_t offset = pci_cfg_read8(device, PCI_CFG_CAP_PTR) & 0xFC;

    while(offset) {
        if(pci_cfg_read8(device, offset) == cap_id)
            return offset;
        offset = pci_cfg_read8(device, offset + 1) & 0xFC;
    }

    return 0;
}

void pci_enable_busmaster(pci_device_t* restrict device)
{
    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);

    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd | PCI_CMD_BUS_MASTER);
}

void pci_disable_busmaster(pci_device_t* restrict device)
{
    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);

    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd & ~(uint16_t)(PCI_CMD_BUS_MASTER));
}

int pci_request_irq(pci_device_t* restrict device, irq_handler_t handler, void* restrict ctxdata)
{
    uint8_t cap = pci_get_cap(device, PCI_CAP_MSI);

    if(cap == 0) {
        return -ENODEV;
    }

    unsigned irq_number = irq_handler_add(UINT_MAX, handler, ctxdata);

    if(irq_number == UINT_MAX) {
        return -ENOMEM;
    }

    device->priv.irq_number = irq_number;

    uint64_t address;
    uint32_t data;
    arch_msi_compose(irq_number + ARCH_IRQ_BASE, &address, &data);

    uint16_t control = pci_cfg_read16(device, cap + PCI_MSI_CONTROL);
    control &= ~(uint16_t)(PCI_MSI_CONTROL_ENABLE | PCI_MSI_CONTROL_MME);

    pci_cfg_write16(device, cap + PCI_MSI_CONTROL, control);
    pci_cfg_write32(device, cap + PCI_MSI_ADDRESS_LO, (uint32_t)(address));

    uintptr_t mask_offset = cap + PCI_MSI_MASK_32;

    if(control & PCI_MSI_CONTROL_64BIT) {
        pci_cfg_write32(device, cap + PCI_MSI_ADDRESS_HI, (uint32_t)(address >> 32));
        pci_cfg_write16(device, cap + PCI_MSI_DATA_64, (uint16_t)(data));
        mask_offset = cap + PCI_MSI_MASK_64;
    }
    else {
        pci_cfg_write16(device, cap + PCI_MSI_DATA_32, (uint16_t)(data));
    }

    if(control & PCI_MSI_CONTROL_PVM) {
        pci_cfg_write32(device, mask_offset, pci_cfg_read32(device, mask_offset) & ~UINT32_C(1));
    }

    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);
    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd | PCI_CMD_INT_DISABLE);

    pci_cfg_write16(device, cap + PCI_MSI_CONTROL, control | PCI_MSI_CONTROL_ENABLE);

    return 0;
}

void pci_release_irq(pci_device_t* restrict device)
{
    if(device->priv.irq_number == UINT_MAX) {
        return;
    }

    uint8_t cap = pci_get_cap(device, PCI_CAP_MSI);

    if(cap) {
        uint16_t control = pci_cfg_read16(device, cap + PCI_MSI_CONTROL);
        pci_cfg_write16(device, cap + PCI_MSI_CONTROL, control & ~(uint16_t)(PCI_MSI_CONTROL_ENABLE));
    }

    irq_handler_remove(device->priv.irq_number);

    device->priv.irq_number = UINT_MAX;
}
