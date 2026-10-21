#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mmio.h>
#include <vex/mutex.h>
#include <vex/string.h>
#include <vex/sysbus.h>
#include <vex/vprot.h>

#define SYSBUS_RESOURCE_MAX 16
#define SYSBUS_ID_MAX       16

typedef struct resource_list_s resource_list_t;

struct resource_list_s {
    fwnode_resource_t resources[SYSBUS_RESOURCE_MAX];
    size_t num_resources;
};

static mutex_t sysbus_mutex = MUTEX_INIT;
static sysbus_device_t* device_head = nullptr;
static sysbus_device_t** device_tail = &device_head;
static sysbus_driver_t* driver_head = nullptr;

static void device_bind(sysbus_device_t* restrict device, sysbus_driver_t* restrict driver)
{
    if(device->priv.driver || !fwnode_matches(&device->priv.node, driver->ids)) {
        return;
    }

    if(0 == driver->ops->probe(device)) {
        device->priv.driver = driver;
    }
}

// A window is what a node passes on to its children, not registers of its own
static void collect_resource(const fwnode_resource_t* resource, void* arg)
{
    resource_list_t* list = arg;
    bool is_range = resource->type != FWNODE_RESOURCE_IRQ && resource->type != FWNODE_RESOURCE_DMA;

    if(is_range && (resource->range.flags & FWNODE_RANGE_WINDOW)) {
        return;
    }

    if(list->num_resources == SYSBUS_RESOURCE_MAX) {
        return;
    }

    list->resources[list->num_resources] = *resource;
    list->num_resources += 1;
}

static int copy_resources(sysbus_device_t* restrict device, const resource_list_t* restrict list)
{
    device->priv.resources = nullptr;
    device->priv.interrupts = nullptr;
    device->priv.num_resources = list->num_resources;

    if(list->num_resources == 0) {
        return 0;
    }

    device->priv.resources = kmalloc(sizeof(fwnode_resource_t) * list->num_resources);
    device->priv.interrupts = kmalloc(sizeof(interrupt_action_t*) * list->num_resources);

    if(device->priv.resources == nullptr || device->priv.interrupts == nullptr) {
        kfree(device->priv.resources);
        kfree(device->priv.interrupts);
        return -ENOMEM;
    }

    memcpy(device->priv.resources, list->resources, sizeof(fwnode_resource_t) * list->num_resources);
    memset(device->priv.interrupts, 0, sizeof(interrupt_action_t*) * list->num_resources);

    return 0;
}

static void add_device(const fwnode_t* restrict node, const char* restrict id)
{
    sysbus_device_t* device = kmalloc(sizeof(sysbus_device_t));

    if(device == nullptr) {
        pr_warning("sysbus: out of memory");
        return;
    }

    resource_list_t list = {};
    fwnode_for_each_resource(node, &collect_resource, &list);

    char* name = kstrdup(id);

    if(name == nullptr) {
        pr_warning("sysbus: out of memory");
        kfree(device);
        return;
    }

    if(copy_resources(device, &list) < 0) {
        pr_warning("sysbus: out of memory");
        kfree(name);
        kfree(device);
        return;
    }

    device->name = name;

    device->priv.next = nullptr;
    device->priv.driver = nullptr;
    device->priv.node = *node;
    device->ctxdata = nullptr;

    *device_tail = device;
    device_tail = &device->priv.next;

    pr_verbose("sysbus: found %s: num_resources=%zu", device->name, device->priv.num_resources);

    for(sysbus_driver_t* it = driver_head; it; it = it->priv.next) {
        device_bind(device, it);
    }
}

// A node another bus claimed is that bus's to describe, with all below it
static void visit_node(const fwnode_t* node, void* arg)
{
    char id[SYSBUS_ID_MAX];

    if(fwnode_is_claimed(node)) {
        return;
    }

    if(fwnode_is_present(node) && 0 == fwnode_get_id(node, id, sizeof(id))) {
        add_device(node, id);
    }

    fwnode_for_each_child(node, &visit_node, nullptr);
}

static fwnode_resource_t* find_resource(sysbus_device_t* restrict device, bool want_irq, unsigned index, size_t* restrict slot)
{
    unsigned found = 0;

    for(size_t i = 0; i < device->priv.num_resources; ++i) {
        fwnode_resource_t* resource = &device->priv.resources[i];
        bool is_irq = resource->type == FWNODE_RESOURCE_IRQ;
        bool is_registers = resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;
        bool wanted = want_irq ? is_irq : is_registers;

        if(!wanted) {
            continue;
        }

        if(found == index) {
            *slot = i;
            return resource;
        }

        found += 1;
    }

    return nullptr;
}

int sysbus_driver_add(sysbus_driver_t* restrict driver)
{
    mutex_acquire(&sysbus_mutex);

    for(sysbus_driver_t* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            mutex_release(&sysbus_mutex);
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(sysbus_device_t* it = device_head; it; it = it->priv.next) {
        device_bind(it, driver);
    }

    mutex_release(&sysbus_mutex);

    return 0;
}

int sysbus_driver_remove(sysbus_driver_t* restrict driver)
{
    mutex_acquire(&sysbus_mutex);

    sysbus_driver_t** prev = &driver_head;

    while(*prev && *prev != driver) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&sysbus_mutex);
        return -ENODEV;
    }

    *prev = driver->priv.next;

    for(sysbus_device_t* it = device_head; it; it = it->priv.next) {
        if(it->priv.driver == driver) {
            if(driver->ops->remove) {
                driver->ops->remove(it);
            }

            it->priv.driver = nullptr;
        }
    }

    mutex_release(&sysbus_mutex);

    return 0;
}

void sysbus_get_node(const sysbus_device_t* restrict device, fwnode_t* restrict node)
{
    *node = device->priv.node;
}

int sysbus_map_io(sysbus_device_t* restrict device, unsigned index, io_region_t* restrict region)
{
    size_t slot;
    const fwnode_resource_t* resource = find_resource(device, false, index, &slot);

    if(resource == nullptr) {
        return -ENODEV;
    }

    if(resource->type == FWNODE_RESOURCE_PORT) {
        region->space = IO_SPACE_PORT;
        region->base = resource->range.start;
        region->length = resource->range.length;
        return 0;
    }

    void* virt = mmio_map(resource->range.start, resource->range.length, VPROT_MMIO);

    if(virt == nullptr) {
        return -ENOMEM;
    }

    region->space = IO_SPACE_MEMORY;
    region->base = (uintptr_t)(virt);
    region->length = resource->range.length;

    return 0;
}

void sysbus_unmap_io(io_region_t* restrict region)
{
    if(region->space == IO_SPACE_MEMORY) {
        mmio_unmap((volatile void*)(region->base));
    }
}

int sysbus_request_irq(sysbus_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata)
{
    size_t slot;
    const fwnode_resource_t* resource = find_resource(device, true, index, &slot);

    if(resource == nullptr) {
        return -ENODEV;
    }

    if(device->priv.interrupts[slot]) {
        return -EBUSY;
    }

    uint32_t gsi = resource->irq.number;
    unsigned trigger = resource->irq.trigger;
    unsigned polarity = resource->irq.polarity;
    unsigned flags = 0;

    if(resource->irq.flags & FWNODE_IRQ_ISA) {
        interrupt_isa_route(resource->irq.number, &gsi, &trigger, &polarity);
    }

    if(resource->irq.flags & FWNODE_IRQ_SHARED) {
        flags |= INTERRUPT_SHARED;
    }

    return interrupt_attach(gsi, trigger, polarity, flags, handler, ctxdata, &device->priv.interrupts[slot]);
}

void sysbus_release_irq(sysbus_device_t* restrict device, unsigned index)
{
    size_t slot;

    if(find_resource(device, true, index, &slot) == nullptr) {
        return;
    }

    if(device->priv.interrupts[slot] == nullptr) {
        return;
    }

    interrupt_detach(device->priv.interrupts[slot]);

    device->priv.interrupts[slot] = nullptr;
}

static void init_sysbus(void)
{
    fwnode_t root;

    if(fwnode_root(&root) < 0) {
        return;
    }

    mutex_acquire(&sysbus_mutex);
    fwnode_for_each_child(&root, &visit_node, nullptr);
    mutex_release(&sysbus_mutex);
}

initcall_bsp_bus(init_sysbus, INIT_LAST);
