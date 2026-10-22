#include <vex/atomic.h>
#include <vex/device.h>
#include <vex/errno.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mmio.h>
#include <vex/mutex.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vprot.h>

#define DEVICE_RESOURCE_MAX 16
#define DEVICE_PATH_MAX     256

typedef struct resource_list_s resource_list_t;

struct device_irq_s {
    device_irq_t* next;
    unsigned index;
    interrupt_action_t* action;
};

struct resource_list_s {
    fwnode_resource_t resources[DEVICE_RESOURCE_MAX];
    size_t num_resources;
};

// Guards the links between nodes
// and the interrupts requested on them
static mutex_t tree_mutex = MUTEX_INIT;
static device_t* root = nullptr;

static bool is_range(const fwnode_resource_t* restrict resource)
{
    return resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;
}

// A window is what a node passes on to its children, not registers of its own
static void collect_resource(const fwnode_resource_t* resource, void* arg)
{
    resource_list_t* list = arg;

    if(is_range(resource) && (resource->range.flags & FWNODE_RANGE_WINDOW)) {
        return;
    }

    if(list->num_resources == DEVICE_RESOURCE_MAX) {
        return;
    }

    list->resources[list->num_resources] = *resource;
    list->num_resources += 1;
}

static int copy_resources(device_t* restrict device)
{
    resource_list_t list = {};

    device->priv.resources = nullptr;
    device->priv.num_resources = 0;

    if(device->fwnode.ops == nullptr) {
        return 0;
    }

    fwnode_for_each_resource(&device->fwnode, &collect_resource, &list);

    if(list.num_resources == 0) {
        return 0;
    }

    device->priv.resources = kmalloc(sizeof(fwnode_resource_t) * list.num_resources);

    if(device->priv.resources == nullptr) {
        return -ENOMEM;
    }

    memcpy(device->priv.resources, list.resources, sizeof(fwnode_resource_t) * list.num_resources);
    device->priv.num_resources = list.num_resources;

    return 0;
}

static device_t* allocate(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name)
{
    device_t* device = kmalloc(sizeof(device_t));

    if(device == nullptr) {
        return nullptr;
    }

    device->fwnode = (fwnode_t) {};

    if(fwnode) {
        device->fwnode = *fwnode;
    }

    if(copy_resources(device) < 0) {
        kfree(device);
        return nullptr;
    }

    device->priv.next = nullptr;
    device->priv.children = nullptr;
    device->priv.irqs = nullptr;
    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);

    device->parent = parent;
    device->bus_ops = nullptr;
    device->bus_ctxdata = nullptr;
    device->ctxdata = nullptr;

    kstrncpy(device->name, name, sizeof(device->name) - 1);
    device->name[sizeof(device->name) - 1] = 0;

    return device;
}

static size_t write_path(const device_t* restrict device, char* restrict buffer, size_t size)
{
    if(device->parent == nullptr) {
        return 0;
    }

    size_t length = write_path(device->parent, buffer, size);
    size_t offset = length < size ? length : size;

    if(length == 0) {
        return length + snprintf(buffer + offset, size - offset, "%s", device->name);
    }
    else {
        return length + snprintf(buffer + offset, size - offset, "/%s", device->name);
    }
}

static int fw_get_resource(device_t* restrict device, bool want_irq, unsigned index, fwnode_resource_t* restrict resource)
{
    unsigned found = 0;

    for(size_t i = 0; i < device->priv.num_resources; ++i) {
        const fwnode_resource_t* candidate = &device->priv.resources[i];
        bool is_irq = candidate->type == FWNODE_RESOURCE_IRQ;
        bool wanted = want_irq ? is_irq : is_range(candidate);

        if(!wanted) {
            continue;
        }

        if(found == index) {
            *resource = *candidate;
            return 0;
        }

        found += 1;
    }

    return -ENODEV;
}

static int get_range(device_t* restrict device, unsigned index, fwnode_resource_t* restrict range)
{
    device_t* bus = device->parent;

    if(bus && bus->bus_ops && bus->bus_ops->get_range) {
        return bus->bus_ops->get_range(bus, device, index, range);
    }

    return fw_get_resource(device, false, index, range);
}

static int get_irq(device_t* restrict device, unsigned index, fwnode_irq_t* restrict irq)
{
    device_t* bus = device->parent;

    if(bus && bus->bus_ops && bus->bus_ops->get_irq) {
        return bus->bus_ops->get_irq(bus, device, index, irq);
    }

    fwnode_resource_t resource;
    int retval = fw_get_resource(device, true, index, &resource);

    if(retval < 0) {
        return retval;
    }

    *irq = resource.irq;

    return 0;
}

static device_irq_t** find_irq_nolock(device_t* restrict device, unsigned index)
{
    device_irq_t** prev = &device->priv.irqs;

    while(*prev && (*prev)->index != index) {
        prev = &(*prev)->next;
    }

    return prev;
}

static int attach_irq(const fwnode_irq_t* restrict irq, irq_handler_t handler, void* restrict ctxdata, interrupt_action_t** restrict action)
{
    uint32_t gsi = irq->number;
    unsigned trigger = irq->trigger;
    unsigned polarity = irq->polarity;
    unsigned flags = 0;

    if(irq->flags & FWNODE_IRQ_ISA) {
        interrupt_isa_route(irq->number, &gsi, &trigger, &polarity);
    }

    if(irq->flags & FWNODE_IRQ_SHARED) {
        flags |= INTERRUPT_SHARED;
    }

    return interrupt_attach(gsi, trigger, polarity, flags, handler, ctxdata, action);
}

device_t* device_get_root(void)
{
    mutex_acquire(&tree_mutex);

    if(root == nullptr) {
        fwnode_t fwnode;
        bool has_fwnode = 0 == fwnode_root(&fwnode);

        root = allocate(nullptr, has_fwnode ? &fwnode : nullptr, "");
    }

    if(root) {
        device_incref(root);
    }

    mutex_release(&tree_mutex);

    return root;
}

int device_add(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name, device_t** restrict device)
{
    device_t* child = allocate(parent, fwnode, name);

    if(child == nullptr) {
        return -ENOMEM;
    }

    device_incref(parent);
    device_incref(child);

    mutex_acquire(&tree_mutex);

    device_t** tail = &parent->priv.children;

    while(*tail) {
        tail = &(*tail)->priv.next;
    }

    *tail = child;

    mutex_release(&tree_mutex);

    char path[DEVICE_PATH_MAX];
    device_get_path(child, path, sizeof(path));
    pr_verbose("device: added %s", path);

    *device = child;

    return 0;
}

void device_incref(device_t* restrict device)
{
    atomic_fetch_add(&device->priv.refcount, 1, ATOMIC_RELAXED);
}

void device_decref(device_t* restrict device)
{
    if(1 != atomic_fetch_sub(&device->priv.refcount, 1, ATOMIC_ACQ_REL)) {
        return;
    }

    kassert(device->priv.children == nullptr);
    kassert(device->priv.irqs == nullptr);

    device_t* parent = device->parent;

    kfree(device->priv.resources);
    kfree(device);

    if(parent) {
        device_decref(parent);
    }
}

size_t device_get_path(const device_t* restrict device, char* restrict buffer, size_t size)
{
    if(size) {
        buffer[0] = 0;
    }

    return write_path(device, buffer, size);
}

int device_translate_range(device_t* restrict bus, unsigned* restrict type, uint64_t* restrict address, uint64_t length)
{
    for(device_t* node = bus; node; node = node->parent) {
        if(node->bus_ops == nullptr || node->bus_ops->translate_range == nullptr) {
            continue;
        }

        int retval = node->bus_ops->translate_range(node, type, address, length);

        if(retval < 0) {
            return retval;
        }
    }

    return 0;
}

int device_map_range(device_t* restrict bus, unsigned type, uint64_t start, uint64_t length, io_region_t* restrict region)
{
    uint64_t address = start;
    int retval = device_translate_range(bus, &type, &address, length);

    if(retval < 0) {
        return retval;
    }

    if(type == FWNODE_RESOURCE_PORT) {
        region->space = IO_SPACE_PORT;
        region->base = address;
        region->length = length;
        return 0;
    }

    void* virt = mmio_map(address, length, VPROT_MMIO);

    if(virt == nullptr) {
        return -ENOMEM;
    }

    region->space = IO_SPACE_MEMORY;
    region->base = (uintptr_t)(virt);
    region->length = length;

    return 0;
}

int device_map_io(device_t* restrict device, unsigned index, io_region_t* restrict region)
{
    fwnode_resource_t range;
    int retval = get_range(device, index, &range);

    if(retval < 0) {
        return retval;
    }

    return device_map_range(device->parent, range.type, range.range.start, range.range.length, region);
}

void device_unmap_io(io_region_t* restrict region)
{
    if(region->space == IO_SPACE_MEMORY) {
        mmio_unmap((volatile void*)(region->base));
    }
}

int device_request_irq(device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata)
{
    fwnode_irq_t irq;
    int retval = get_irq(device, index, &irq);

    if(retval < 0) {
        return retval;
    }

    device_irq_t* entry = kmalloc(sizeof(device_irq_t));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&tree_mutex);

    if(*find_irq_nolock(device, index)) {
        mutex_release(&tree_mutex);
        kfree(entry);
        return -EBUSY;
    }

    retval = attach_irq(&irq, handler, ctxdata, &entry->action);

    if(retval < 0) {
        mutex_release(&tree_mutex);
        kfree(entry);
        return retval;
    }

    entry->index = index;
    entry->next = device->priv.irqs;
    device->priv.irqs = entry;

    mutex_release(&tree_mutex);

    return 0;
}

void device_release_irq(device_t* restrict device, unsigned index)
{
    mutex_acquire(&tree_mutex);

    device_irq_t** prev = find_irq_nolock(device, index);
    device_irq_t* entry = *prev;

    if(entry) {
        *prev = entry->next;
    }

    mutex_release(&tree_mutex);

    if(entry) {
        interrupt_detach(entry->action);
        kfree(entry);
    }
}
