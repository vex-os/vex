#include <vex/device.h>
#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/initcall.h>
#include <vex/isa.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/pci.h>
#include <vex/sprintf.h>
#include <vex/string.h>

#define ISA_ID_MAX 16

typedef struct first_range_s first_range_t;

struct isa_bus_s {
    pci_device_t* bridge;
};

struct first_range_s {
    uint64_t start;
    bool found;
};

static const pci_driver_id_t bridge_ids[] = {
    PCI_DEVICE_CLASS(0x06, 0x01),
    PCI_DEVICE_END(),
};

static mutex_t isa_mutex = MUTEX_INIT;
static isa_device_t* device_head = nullptr;
static isa_device_t** device_tail = &device_head;
static isa_driver_t* driver_head = nullptr;

static void device_bind(isa_device_t* restrict device, isa_driver_t* restrict driver)
{
    if(device->priv.driver || !fwnode_matches(&device->priv.device->fwnode, driver->ids)) {
        return;
    }

    if(0 == driver->ops->probe(device)) {
        device->priv.driver = driver;
    }
}

static void find_first_range(const fwnode_resource_t* resource, void* arg)
{
    first_range_t* first = arg;
    bool is_range = resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;

    if(first->found || !is_range) {
        return;
    }

    first->start = resource->range.start;
    first->found = true;
}

// A device is named after its first register range on the bus, as isa:3f8,
// and only one without registers falls back to the firmware's name for it
static int make_name(const fwnode_t* restrict node, char* restrict name, size_t size)
{
    first_range_t first = {};
    fwnode_for_each_resource(node, &find_first_range, &first);

    if(!first.found) {
        return fwnode_get_name(node, name, size);
    }

    unsigned long long start = first.start;
    snprintf(name, size, "isa:%llx", start);

    return 0;
}

static void add_device(isa_bus_t* restrict bus, const fwnode_t* restrict node, const char* restrict id)
{
    char name[DEVICE_NAME_MAX];

    if(make_name(node, name, sizeof(name)) < 0) {
        return;
    }

    isa_device_t* isa_device = kmalloc(sizeof(isa_device_t));
    char* id_copy = kstrdup(id);
    device_t* device = nullptr;

    if(isa_device == nullptr || id_copy == nullptr || device_add(bus->bridge->priv.device, node, name, &device) < 0) {
        pr_warning("isa: out of memory");
        kfree(id_copy);
        kfree(isa_device);
        return;
    }

    device->bus_ctxdata = isa_device;

    isa_device->name = id_copy;
    isa_device->priv.next = nullptr;
    isa_device->priv.bus = bus;
    isa_device->priv.driver = nullptr;
    isa_device->priv.device = device;
    isa_device->ctxdata = nullptr;

    *device_tail = isa_device;
    device_tail = &isa_device->priv.next;

    for(isa_driver_t* it = driver_head; it; it = it->priv.next) {
        device_bind(isa_device, it);
    }
}

static int isa_place_child(device_t* restrict bridge, const fwnode_t* restrict child)
{
    isa_bus_t* bus = bridge->ctxdata;
    char id[ISA_ID_MAX];

    if(fwnode_get_id(child, id, sizeof(id)) < 0) {
        return -ENOENT;
    }

    mutex_acquire(&isa_mutex);
    add_device(bus, child, id);
    mutex_release(&isa_mutex);

    return 0;
}

static const device_bus_ops_t isa_bus_ops = {
    .place_child = &isa_place_child,
};

// UNDONE: a bridge the firmware doesn't describe, such
// as one on a card, needs its devices found through ISA PnP
static int bridge_probe(pci_device_t* restrict bridge, const pci_driver_id_t* restrict id)
{
    device_t* device = bridge->priv.device;

    if(device->fwnode.ops == nullptr) {
        return -ENODEV;
    }

    isa_bus_t* bus = kmalloc(sizeof(isa_bus_t));

    if(bus == nullptr) {
        return -ENOMEM;
    }

    bus->bridge = bridge;
    bridge->ctxdata = bus;
    device->ctxdata = bus;
    device->bus_ops = &isa_bus_ops;

    return 0;
}

static const pci_driver_ops_t bridge_ops = {
    .probe = &bridge_probe,
};

static pci_driver_t bridge_driver = {
    .ops = &bridge_ops,
    .ids = bridge_ids,
};

int isa_driver_add(isa_driver_t* restrict driver)
{
    mutex_acquire(&isa_mutex);

    for(isa_driver_t* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            mutex_release(&isa_mutex);
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(isa_device_t* it = device_head; it; it = it->priv.next) {
        device_bind(it, driver);
    }

    mutex_release(&isa_mutex);

    return 0;
}

int isa_driver_remove(isa_driver_t* restrict driver)
{
    mutex_acquire(&isa_mutex);

    isa_driver_t** prev = &driver_head;

    while(*prev && *prev != driver) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&isa_mutex);
        return -ENODEV;
    }

    *prev = driver->priv.next;

    for(isa_device_t* it = device_head; it; it = it->priv.next) {
        if(it->priv.driver == driver) {
            if(driver->ops->remove) {
                driver->ops->remove(it);
            }

            it->priv.driver = nullptr;
        }
    }

    mutex_release(&isa_mutex);

    return 0;
}

void isa_get_node(const isa_device_t* restrict device, fwnode_t* restrict node)
{
    *node = device->priv.device->fwnode;
}

int isa_map_io(isa_device_t* restrict device, unsigned index, io_region_t* restrict region)
{
    return device_map_io(device->priv.device, index, region);
}

void isa_unmap_io(io_region_t* restrict region)
{
    device_unmap_io(region);
}

int isa_request_irq(isa_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata)
{
    return device_request_irq(device->priv.device, index, handler, ctxdata);
}

void isa_release_irq(isa_device_t* restrict device, unsigned index)
{
    device_release_irq(device->priv.device, index);
}

static void init_isa(void)
{
    pci_driver_add(&bridge_driver);
}

initcall_bsp_bus(init_isa, INIT_MIDDLE);
