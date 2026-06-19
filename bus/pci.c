#include <arch/ireq.h>
#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/page.h>
#include <vex/pci.h>
#include <vex/pmap.h>
#include <vex/vprot.h>

#define PCI_MAX_BUS_DEVICES 32

static struct pci_bus* bus_head = nullptr;
static struct pci_driver* driver_head = nullptr;

static void device_bind(struct pci_device* restrict device, struct pci_driver* restrict driver)
{
    if(device->priv.driver) {
        return;
    }

    for(const struct pci_driver_id* id = driver->ids; id->vendor_id < 0xFFFF || id->device_id < 0xFFFF; id += 1) {
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

static void bus_scan(struct pci_bus* restrict bus)
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
            struct pci_device* device = kmalloc(sizeof(struct pci_device));

            if(device == nullptr) {
                continue;
            }

            unsigned class_id = bus->ops->read8(bus, dev, fn, PCI_CFG_CLASS);
            unsigned subclass_id = bus->ops->read8(bus, dev, fn, PCI_CFG_SUBCLASS);

            device->priv.next = nullptr;
            device->priv.driver = nullptr;
            device->priv.ireq_number = UINT_MAX;
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

            pr_verbose("pci: found %04X:%04X at %s:%02X:%02X", vendor_id, device_id, bus->identifier, dev, fn);

            for(struct pci_driver* it = driver_head; it; it = it->priv.next) {
                device_bind(device, it);
            }
        }
    }
}

static uintptr_t bar_address(struct pci_device* restrict device, unsigned bar_index)
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

static size_t bar_length(struct pci_device* restrict device, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_cfg_read32(device, offset);
    pci_cfg_write32(device, offset, UINT32_C(0xFFFFFFFF));

    uint32_t mask = pci_cfg_read32(device, offset);
    pci_cfg_write32(device, offset, raw);

    if(raw & PCI_BAR_IO) {
        mask &= PCI_BAR_ADDR_MASK_IO;
    }
    else {
        mask &= PCI_BAR_ADDR_MASK_MEM;
    }

    return (size_t)(~mask) + 1;
}

static void* bar_map_mmio(struct pci_device* restrict device, unsigned bar_index)
{
    uintptr_t phys = bar_address(device, bar_index);
    size_t size = bar_length(device, bar_index);

    for(size_t i = 0; i < size; i += PAGE_SIZE) {
        pmap_map(&sys_pmap, (uintptr_t)phys_to_virt(phys + i), phys + i, VPROT_MMIO);
    }

    return phys_to_virt(phys);
}

static void bar_unmap_mmio(struct pci_device* restrict device, unsigned bar_index)
{
    uintptr_t phys = bar_address(device, bar_index);
    size_t size = bar_length(device, bar_index);

    for(size_t i = 0; i < size; i += PAGE_SIZE) {
        pmap_unmap(&sys_pmap, (uintptr_t)phys_to_virt(phys + i));
    }
}

int add_pci_bus(struct pci_bus* restrict bus)
{
    for(struct pci_bus* it = bus_head; it; it = it->priv.next) {
        if(it == bus) {
            return -EEXIST;
        }
    }

    bus->priv.devices = nullptr;
    bus->priv.next = bus_head;
    bus_head = bus;

    bus_scan(bus);

    return 0;
}

int remove_pci_bus(struct pci_bus* restrict bus)
{
    if(bus_head == bus) {
        bus_head = bus->priv.next;
    }
    else {
        struct pci_bus* it;
        struct pci_bus* prev = nullptr;

        for(it = bus_head; it; it = it->priv.next) {
            if(it == bus) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return -ENODEV;
        }
    }

    for(struct pci_device* it = bus->priv.devices; it;) {
        struct pci_device* next = it->priv.next;

        if(it->priv.driver && it->priv.driver->ops->remove) {
            it->priv.driver->ops->remove(it);
        }

        kfree(it);

        it = next;
    }

    return 0;
}

int add_pci_driver(struct pci_driver* restrict driver)
{
    for(struct pci_driver* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(struct pci_bus* bus = bus_head; bus; bus = bus->priv.next) {
        for(struct pci_device* it = bus->priv.devices; it; it = it->priv.next) {
            device_bind(it, driver);
        }
    }

    return 0;
}

int remove_pci_driver(struct pci_driver* restrict driver)
{
    if(driver_head == driver) {
        driver_head = driver->priv.next;
    }
    else {
        struct pci_driver* it;
        struct pci_driver* prev = nullptr;

        for(it = driver_head; it; it = it->priv.next) {
            if(it == driver) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return -ENODEV;
        }
    }

    if(driver->ops->remove) {
        for(struct pci_bus* bus = bus_head; bus; bus = bus->priv.next) {
            for(struct pci_device* it = bus->priv.devices; it; it = it->priv.next) {
                if(it->priv.driver == driver) {
                    driver->ops->remove(it);
                    it->priv.driver = nullptr;
                }
            }
        }
    }

    return 0;
}

struct pci_device* pci_find(unsigned segment, unsigned bus, unsigned device, unsigned function)
{
    for(struct pci_bus* it = bus_head; it; it = it->priv.next) {
        if(it->segment == segment && it->index == bus) {
            for(struct pci_device* dev = it->priv.devices; dev; dev = dev->priv.next) {
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
    kassert(bar_index < 6);

    if(device->priv.bars_mapped & (1U << bar_index)) {
        return &device->priv.bars[bar_index];
    }

    struct pci_bar* bar = &device->priv.bars[bar_index];
    uint32_t raw = pci_cfg_read32(device, PCI_CFG_BAR0 + bar_index * 4);

    if(raw & PCI_BAR_IO) {
        bar->type = PCI_BAR_TYPE_PMIO;
        bar->pmio.base = raw & PCI_BAR_ADDR_MASK_IO;
        bar->pmio.length = bar_length(device, bar_index);
        pmio_region_setup(&bar->pmio);
    }
    else {
        size_t size = bar_length(device, bar_index);
        void* virt = bar_map_mmio(device, bar_index);

        bar->type = PCI_BAR_TYPE_MMIO;
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
    kassert(bar_index < 6);

    if(device->priv.bars_mapped & (1U << bar_index)) {
        struct pci_bar* bar = &device->priv.bars[bar_index];

        if(bar->type == PCI_BAR_TYPE_MMIO) {
            bar_unmap_mmio(device, bar_index);
        }

        device->priv.bars_mapped &= ~(1U << bar_index);
    }
}

uint8_t pci_bar_read8(struct pci_bar* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return pmio_read8(&bar->pmio, offset);
    return bar->mmio.virt8[offset];
}

uint16_t pci_bar_read16(struct pci_bar* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return pmio_read16(&bar->pmio, offset);
    return bar->mmio.virt16[offset / 2];
}

uint32_t pci_bar_read32(struct pci_bar* restrict bar, uintptr_t offset)
{
    if(bar->type == PCI_BAR_TYPE_PMIO)
        return pmio_read32(&bar->pmio, offset);
    return bar->mmio.virt32[offset / 4];
}

uint64_t pci_bar_read64(struct pci_bar* restrict bar, uintptr_t offset)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);
    return bar->mmio.virt64[offset / 8];
}

void pci_bar_read8s(struct pci_bar* restrict bar, uintptr_t offset, uint8_t* restrict data, size_t count)
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

void pci_bar_read16s(struct pci_bar* restrict bar, uintptr_t offset, uint16_t* restrict data, size_t count)
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

void pci_bar_read32s(struct pci_bar* restrict bar, uintptr_t offset, uint32_t* restrict data, size_t count)
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

void pci_bar_read64s(struct pci_bar* restrict bar, uintptr_t offset, uint64_t* restrict data, size_t count)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);

    for(size_t i = 0; i < count; ++i) {
        data[i] = bar->mmio.virt64[(offset / 8) + i];
    }
}

void pci_bar_write8(struct pci_bar* restrict bar, uintptr_t offset, uint8_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write8(&bar->pmio, offset, value);
    }
    else {
        bar->mmio.virt8[offset] = value;
    }
}

void pci_bar_write16(struct pci_bar* restrict bar, uintptr_t offset, uint16_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write16(&bar->pmio, offset, value);
    }
    else {
        bar->mmio.virt16[offset / 2] = value;
    }
}

void pci_bar_write32(struct pci_bar* restrict bar, uintptr_t offset, uint32_t value)
{
    if(bar->type == PCI_BAR_TYPE_PMIO) {
        pmio_write32(&bar->pmio, offset, value);
    }
    else {
        bar->mmio.virt32[offset / 4] = value;
    }
}

void pci_bar_write64(struct pci_bar* restrict bar, uintptr_t offset, uint64_t value)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);

    bar->mmio.virt64[offset / 8] = value;
}

void pci_bar_write8s(struct pci_bar* restrict bar, uintptr_t offset, const uint8_t* restrict data, size_t count)
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

void pci_bar_write16s(struct pci_bar* restrict bar, uintptr_t offset, const uint16_t* restrict data, size_t count)
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

void pci_bar_write32s(struct pci_bar* restrict bar, uintptr_t offset, const uint32_t* restrict data, size_t count)
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

void pci_bar_write64s(struct pci_bar* restrict bar, uintptr_t offset, const uint64_t* restrict data, size_t count)
{
    kassert(bar->type == PCI_BAR_TYPE_MMIO);

    for(size_t i = 0; i < count; ++i) {
        bar->mmio.virt64[(offset / 8) + i] = data[i];
    }
}

uint8_t pci_get_cap(struct pci_device* restrict device, uint8_t cap_id)
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
    uint8_t int_pin = pci_cfg_read8(device, PCI_CFG_INT_PIN);

    if(int_pin == 0) {
        return -ENODEV;
    }

    uint8_t int_line = pci_cfg_read8(device, PCI_CFG_INT_LINE);

    if(int_line == 0xFF) {
        return -ENODEV;
    }

    struct intctl* controller = find_intctl(int_line);

    if(controller == nullptr) {
        return -ENODEV;
    }

    unsigned ireq_number = add_ireq_handler(UINT_MAX, handler, ctxdata);

    if(ireq_number == UINT_MAX) {
        return -ENOMEM;
    }

    device->priv.ireq_number = ireq_number;

    intctl_route(controller, int_line, ireq_number + ARCH_IREQ_BASE, 0, TRIGGER_LEVEL, ACTIVE_LOW);
    intctl_unmask(controller, int_line);

    return 0;
}

void pci_release_irq(struct pci_device* restrict device)
{
    if(device->priv.ireq_number == UINT_MAX) {
        return;
    }

    uint8_t int_line = pci_cfg_read8(device, PCI_CFG_INT_LINE);
    struct intctl* controller = find_intctl(int_line);

    if(controller) {
        intctl_mask(controller, int_line);
    }

    remove_ireq_handler(device->priv.ireq_number);

    device->priv.ireq_number = UINT_MAX;
}
