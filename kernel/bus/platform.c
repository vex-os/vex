#include <vex/errno.h>
#include <vex/firmware.h>
#include <vex/initcall.h>
#include <vex/isa.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mmio.h>
#include <vex/mutex.h>
#include <vex/platform.h>
#include <vex/string.h>
#include <vex/vprot.h>

#define PLATFORM_RESOURCE_MAX 16
#define PLATFORM_ID_MAX       16

typedef struct resource_list_s resource_list_t;

struct resource_list_s {
    firmware_resource_t resources[PLATFORM_RESOURCE_MAX];
    size_t num_resources;
};

static mutex_t platform_mutex = MUTEX_INIT;
static platform_device_t* device_head = nullptr;
static platform_device_t** device_tail = &device_head;
static platform_driver_t* driver_head = nullptr;

static void device_bind(platform_device_t* restrict device, platform_driver_t* restrict driver)
{
    if(device->priv.driver || !firmware_node_matches(&device->priv.node, driver->ids)) {
        return;
    }

    if(0 == driver->ops->probe(device)) {
        device->priv.driver = driver;
    }
}

// A window is what a node passes on to its children, not registers of its own
static void collect_resource(const firmware_resource_t* resource, void* arg)
{
    resource_list_t* list = arg;
    bool is_range = resource->type != FIRMWARE_RESOURCE_IRQ && resource->type != FIRMWARE_RESOURCE_DMA;

    if(is_range && (resource->range.flags & FIRMWARE_RANGE_WINDOW)) {
        return;
    }

    if(list->num_resources == PLATFORM_RESOURCE_MAX) {
        return;
    }

    list->resources[list->num_resources] = *resource;
    list->num_resources += 1;
}

static int copy_resources(platform_device_t* restrict device, const resource_list_t* restrict list)
{
    device->priv.resources = nullptr;
    device->priv.interrupts = nullptr;
    device->priv.num_resources = list->num_resources;

    if(list->num_resources == 0) {
        return 0;
    }

    device->priv.resources = kmalloc(sizeof(firmware_resource_t) * list->num_resources);
    device->priv.interrupts = kmalloc(sizeof(interrupt_action_t*) * list->num_resources);

    if(device->priv.resources == nullptr || device->priv.interrupts == nullptr) {
        kfree(device->priv.resources);
        kfree(device->priv.interrupts);
        return -ENOMEM;
    }

    memcpy(device->priv.resources, list->resources, sizeof(firmware_resource_t) * list->num_resources);
    memset(device->priv.interrupts, 0, sizeof(interrupt_action_t*) * list->num_resources);

    return 0;
}

static void add_device(const firmware_node_t* restrict node, const char* restrict id)
{
    platform_device_t* device = kmalloc(sizeof(platform_device_t));

    if(device == nullptr) {
        pr_warning("platform: out of memory");
        return;
    }

    resource_list_t list = {};
    firmware_node_for_each_resource(node, &collect_resource, &list);

    char* name = kstrdup(id);

    if(name == nullptr) {
        pr_warning("platform: out of memory");
        kfree(device);
        return;
    }

    if(copy_resources(device, &list) < 0) {
        pr_warning("platform: out of memory");
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

    pr_verbose("platform: found %s: num_resources=%zu", device->name, device->priv.num_resources);

    for(platform_driver_t* it = driver_head; it; it = it->priv.next) {
        device_bind(device, it);
    }
}

// A node another bus claimed is that bus's to describe, with all below it
static void visit_node(const firmware_node_t* node, void* arg)
{
    char id[PLATFORM_ID_MAX];

    if(firmware_node_is_claimed(node)) {
        return;
    }

    if(firmware_node_is_present(node) && 0 == firmware_node_get_id(node, id, sizeof(id))) {
        add_device(node, id);
    }

    firmware_node_for_each_child(node, &visit_node, nullptr);
}

static firmware_resource_t* find_resource(platform_device_t* restrict device, bool want_irq, unsigned index, size_t* restrict slot)
{
    unsigned found = 0;

    for(size_t i = 0; i < device->priv.num_resources; ++i) {
        firmware_resource_t* resource = &device->priv.resources[i];
        bool is_irq = resource->type == FIRMWARE_RESOURCE_IRQ;
        bool is_registers = resource->type == FIRMWARE_RESOURCE_PORT || resource->type == FIRMWARE_RESOURCE_MEMORY;
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

int platform_driver_add(platform_driver_t* restrict driver)
{
    mutex_acquire(&platform_mutex);

    for(platform_driver_t* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            mutex_release(&platform_mutex);
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(platform_device_t* it = device_head; it; it = it->priv.next) {
        device_bind(it, driver);
    }

    mutex_release(&platform_mutex);

    return 0;
}

int platform_driver_remove(platform_driver_t* restrict driver)
{
    mutex_acquire(&platform_mutex);

    platform_driver_t** prev = &driver_head;

    while(*prev && *prev != driver) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&platform_mutex);
        return -ENODEV;
    }

    *prev = driver->priv.next;

    for(platform_device_t* it = device_head; it; it = it->priv.next) {
        if(it->priv.driver == driver) {
            if(driver->ops->remove) {
                driver->ops->remove(it);
            }

            it->priv.driver = nullptr;
        }
    }

    mutex_release(&platform_mutex);

    return 0;
}

void platform_get_node(const platform_device_t* restrict device, firmware_node_t* restrict node)
{
    *node = device->priv.node;
}

int platform_map_io(platform_device_t* restrict device, unsigned index, io_region_t* restrict region)
{
    size_t slot;
    const firmware_resource_t* resource = find_resource(device, false, index, &slot);

    if(resource == nullptr) {
        return -ENODEV;
    }

    if(resource->type == FIRMWARE_RESOURCE_PORT) {
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

void platform_unmap_io(io_region_t* restrict region)
{
    if(region->space == IO_SPACE_MEMORY) {
        mmio_unmap((volatile void*)(region->base));
    }
}

int platform_request_irq(platform_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata)
{
    size_t slot;
    const firmware_resource_t* resource = find_resource(device, true, index, &slot);

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

    if(resource->irq.flags & FIRMWARE_IRQ_ISA) {
        isa_irq_route(resource->irq.number, &gsi, &trigger, &polarity);
    }

    if(resource->irq.flags & FIRMWARE_IRQ_SHARED) {
        flags |= INTERRUPT_SHARED;
    }

    return interrupt_attach(gsi, trigger, polarity, flags, handler, ctxdata, &device->priv.interrupts[slot]);
}

void platform_release_irq(platform_device_t* restrict device, unsigned index)
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

static void init_platform(void)
{
    firmware_node_t root;

    if(firmware_root(&root) < 0) {
        return;
    }

    mutex_acquire(&platform_mutex);
    firmware_node_for_each_child(&root, &visit_node, nullptr);
    mutex_release(&platform_mutex);
}

initcall_bsp_bus(init_platform, INIT_LAST);
