#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>

#include "device_internal.h"

#define DEVICE_RESOURCE_MAX 16

typedef struct resource_list_s resource_list_t;

struct resource_list_s {
    fwnode_resource_t resources[DEVICE_RESOURCE_MAX];
    size_t num_resources;
};

mutex_t device_tree_mutex = MUTEX_INIT;
static device_t* root = nullptr;
static uint64_t next_id = 1; // The root is 0, which /dev/devctl gives as the parent of its children

// A window is what a node passes on to its children, not registers of its own
static void collect_resource(const fwnode_resource_t* resource, void* arg)
{
    resource_list_t* list = arg;

    if(device_is_range(resource) && (resource->range.flags & FWNODE_RANGE_WINDOW)) {
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
    device->priv.mappings = nullptr;
    device->priv.providers = nullptr;
    device->priv.consumers = nullptr;
    device->priv.waits_for = nullptr;
    device->priv.waits_for_fwnode = (fwnode_t) {};
    device->priv.waiting_next = nullptr;
    device->priv.worker_next = nullptr;
    device->priv.pending = 0;
    device->priv.state = DEVICE_PRESENT;
    device->priv.unplaced = false;
    device->priv.gone = false;
    device->priv.offline = false;
    device->priv.attach_failed = false;
    atomic_store(&device->priv.refcount, 1, ATOMIC_RELAXED);

    device->parent = parent;
    device->id = parent ? atomic_fetch_add(&next_id, 1, ATOMIC_RELAXED) : 0;
    device->bus_ops = nullptr;
    device->bus_data = nullptr;
    device->driver = nullptr;
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

static int add_node(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name, void* restrict bus_data,
    bool unplaced, device_t** restrict device)
{
    device_t* child = allocate(parent, fwnode, name);

    if(child == nullptr) {
        return -ENOMEM;
    }

    child->bus_data = bus_data;
    child->priv.unplaced = unplaced;

    device_incref(parent);
    device_incref(child);

    mutex_acquire(&device_tree_mutex);

    device_t** tail = &parent->priv.children;

    while(*tail) {
        tail = &(*tail)->priv.next;
    }

    *tail = child;

    mutex_release(&device_tree_mutex);

    char path[DEVICE_PATH_MAX];
    device_get_path(child, path, sizeof(path));
    pr_verbose("device: added %s", path);

    devctl_node_added(child);

    *device = child;

    return 0;
}

static device_t* find_child_by_fwnode(device_t* restrict parent, const fwnode_t* restrict fwnode)
{
    mutex_acquire(&device_tree_mutex);

    device_t* child = parent->priv.children;

    while(child && (child->fwnode.ops == nullptr || !fwnode_is_same(&child->fwnode, fwnode))) {
        child = child->priv.next;
    }

    if(child) {
        device_incref(child);
    }

    mutex_release(&device_tree_mutex);

    return child;
}

static bool firmware_judges(const device_t* restrict parent, const device_t* restrict child)
{
    return child->priv.unplaced || parent->bus_ops == nullptr || parent->bus_ops->rescan == nullptr;
}

static void place_firmware_child(device_t* restrict parent, const fwnode_t* restrict child)
{
    if(parent->bus_ops && parent->bus_ops->place_child) {
        int retval = parent->bus_ops->place_child(parent, child);

        if(retval != -ENOENT) {
            return;
        }
    }

    char name[DEVICE_NAME_MAX];

    if(fwnode_get_name(child, name, sizeof(name)) < 0) {
        return;
    }

    device_t* device = nullptr;

    if(add_node(parent, child, name, nullptr, true, &device) < 0) {
        pr_warning("device: out of memory");
        return;
    }

    device_decref(device);
}

static void sync_firmware_child(const fwnode_t* child, void* arg)
{
    device_t* parent = arg;
    device_t* device = find_child_by_fwnode(parent, child);
    bool present = fwnode_is_present(child);

    if(device == nullptr) {
        if(present) {
            place_firmware_child(parent, child);
        }

        return;
    }

    if(!present && firmware_judges(parent, device)) {
        device_set_gone(device);
        device_remove(device);
    }

    device_decref(device);
}

static void set_gone(device_t* device)
{
    device->priv.gone = true;

    for(device_t* child = device->priv.children; child; child = child->priv.next) {
        set_gone(child);
    }
}

static device_t* find_by_fwnode_nolock(device_t* device, const fwnode_t* restrict fwnode)
{
    if(device->fwnode.ops && fwnode_is_same(&device->fwnode, fwnode)) {
        return device;
    }

    for(device_t* child = device->priv.children; child; child = child->priv.next) {
        device_t* found = find_by_fwnode_nolock(child, fwnode);

        if(found) {
            return found;
        }
    }

    return nullptr;
}

// The walks over the tree take no restrict, as removing a child changes its parent
device_t* device_next_child(device_t* parent, device_t* prev)
{
    mutex_acquire(&device_tree_mutex);

    device_t* child = parent->priv.children;

    if(prev) {
        child = prev->priv.next;
    }

    if(child) {
        device_incref(child);
    }

    mutex_release(&device_tree_mutex);

    return child;
}

device_t* device_find_by_fwnode(const fwnode_t* restrict fwnode)
{
    mutex_acquire(&device_tree_mutex);

    device_t* device = nullptr;

    if(root) {
        device = find_by_fwnode_nolock(root, fwnode);
    }

    if(device) {
        device_incref(device);
    }

    mutex_release(&device_tree_mutex);

    return device;
}

void device_sync_firmware_children(device_t* restrict device)
{
    fwnode_for_each_child(&device->fwnode, &sync_firmware_child, device);
}

device_t* device_get_root(void)
{
    mutex_acquire(&device_tree_mutex);

    if(root == nullptr) {
        fwnode_t fwnode;
        bool has_fwnode = 0 == fwnode_root(&fwnode);

        root = allocate(nullptr, has_fwnode ? &fwnode : nullptr, "");
    }

    if(root) {
        device_incref(root);
    }

    mutex_release(&device_tree_mutex);

    return root;
}

int device_add(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name, void* restrict bus_data,
    device_t** restrict device)
{
    return add_node(parent, fwnode, name, bus_data, false, device);
}

void device_set_gone(device_t* restrict device)
{
    mutex_acquire(&device_tree_mutex);
    set_gone(device);
    mutex_release(&device_tree_mutex);
}

bool device_is_gone(const device_t* restrict device)
{
    return device->priv.gone || device->priv.state == DEVICE_GONE;
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
    kassert(device->priv.mappings == nullptr);
    kassert(device->priv.providers == nullptr);
    kassert(device->priv.consumers == nullptr);
    kassert(device->priv.waits_for == nullptr);

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
