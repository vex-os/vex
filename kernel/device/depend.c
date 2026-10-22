#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>

#include "device_internal.h"

typedef struct dependency_walk_s dependency_walk_t;

struct dependency_walk_s {
    device_t* device;
    int retval;
};

// Firmware lists the power engine plugin (Windows' PEP) in _DEP for
// the OS's power management, which no driver of the tree stands for
static const char* const ignored_providers[] = {
    "PNP0D80",
    nullptr,
};

static device_t* waiting_head = nullptr; // Under the tree's lock

// A node relies on its parent, on the providers its driver uses and on the one it waits for
static bool relies_on_nolock(const device_t* device, const device_t* provider)
{
    if(device == provider) {
        return true;
    }

    if(device->parent && relies_on_nolock(device->parent, provider)) {
        return true;
    }

    for(const device_link_t* link = device->priv.providers; link; link = link->next_of_consumer) {
        if(relies_on_nolock(link->provider, provider)) {
            return true;
        }
    }

    return device->priv.waits_for && relies_on_nolock(device->priv.waits_for, provider);
}

// A provider the firmware says isn't there is one the node does without,
// as the firmware would then have it work without it
static int wait_for_fwnode(device_t* restrict device, const fwnode_t* restrict fwnode)
{
    if(fwnode_matches(fwnode, ignored_providers) || !fwnode_is_present(fwnode)) {
        return 0;
    }

    device_t* provider = device_find_by_fwnode(fwnode);

    if(provider == nullptr) {
        device->priv.waits_for_fwnode = *fwnode;
        return -EAGAIN;
    }

    int retval = device_wait_for(device, provider);

    device_decref(provider);

    return retval;
}

static void wait_for_dependency(const fwnode_t* provider, void* arg)
{
    dependency_walk_t* walk = arg;

    if(walk->retval == 0) {
        walk->retval = wait_for_fwnode(walk->device, provider);
    }
}

static bool waits_for_nolock(const device_t* restrict waiter, const device_t* restrict provider)
{
    if(waiter->priv.waits_for == provider) {
        return true;
    }

    return waiter->priv.waits_for_fwnode.ops && provider->fwnode.ops && fwnode_is_same(&waiter->priv.waits_for_fwnode, &provider->fwnode);
}

bool device_is_waiting(const device_t* restrict device)
{
    return device->priv.waits_for || device->priv.waits_for_fwnode.ops;
}

int device_wait_for_dependencies(device_t* restrict device)
{
    dependency_walk_t walk = {};
    walk.device = device;

    if(device->fwnode.ops) {
        fwnode_for_each_dependency(&device->fwnode, &wait_for_dependency, &walk);
    }

    return walk.retval;
}

void device_unlink_providers(device_t* restrict device)
{
    mutex_acquire(&device_tree_mutex);

    device_link_t* links = device->priv.providers;
    device->priv.providers = nullptr;

    for(device_link_t* link = links; link; link = link->next_of_consumer) {
        device_link_t** prev = &link->provider->priv.consumers;

        while(*prev != link) {
            prev = &(*prev)->next_of_provider;
        }

        *prev = link->next_of_provider;
    }

    mutex_release(&device_tree_mutex);

    while(links) {
        device_link_t* next = links->next_of_consumer;
        device_decref(links->provider);
        kfree(links);
        links = next;
    }
}

void device_start_waiting(device_t* restrict device)
{
    mutex_acquire(&device_tree_mutex);
    device->priv.waiting_next = waiting_head;
    waiting_head = device;
    device->priv.state = DEVICE_WAITING;
    mutex_release(&device_tree_mutex);
}

void device_stop_waiting(device_t* restrict device)
{
    mutex_acquire(&device_tree_mutex);

    device_t* provider = device->priv.waits_for;
    device->priv.waits_for = nullptr;
    device->priv.waits_for_fwnode = (fwnode_t) {};

    if(device->priv.state == DEVICE_WAITING) {
        device_t** prev = &waiting_head;

        while(*prev != device) {
            prev = &(*prev)->priv.waiting_next;
        }

        *prev = device->priv.waiting_next;
        device->priv.waiting_next = nullptr;
        device->priv.state = DEVICE_PRESENT;
    }

    mutex_release(&device_tree_mutex);

    if(provider) {
        device_decref(provider);
    }
}

// When the provider is bound or leaves the tree, so its waiters
// either attach or find out what they wait for won't come
void device_wake_waiters(device_t* restrict provider)
{
    mutex_acquire(&device_tree_mutex);

    device_t** prev = &waiting_head;

    while(*prev) {
        device_t* waiter = *prev;

        if(!waits_for_nolock(waiter, provider)) {
            prev = &waiter->priv.waiting_next;
            continue;
        }

        *prev = waiter->priv.waiting_next;
        waiter->priv.waiting_next = nullptr;
        waiter->priv.waits_for_fwnode = (fwnode_t) {};
        waiter->priv.state = DEVICE_PRESENT;

        if(waiter->priv.waits_for) {
            waiter->priv.waits_for = nullptr;
            device_decref(provider);
        }

        device_queue_request(waiter, DEVICE_REQUEST_RESCAN);
    }

    mutex_release(&device_tree_mutex);
}

void device_log_wait(device_t* restrict device, const char* restrict path, const driver_t* restrict driver)
{
    if(device->priv.waits_for) {
        char provider_path[DEVICE_PATH_MAX];
        device_get_path(device->priv.waits_for, provider_path, sizeof(provider_path));
        pr_verbose("device: %s: %s waits for %s", path, driver->name, provider_path);
    }
    else {
        char provider_name[DEVICE_NAME_MAX];

        if(fwnode_get_name(&device->priv.waits_for_fwnode, provider_name, sizeof(provider_name)) < 0) {
            provider_name[0] = 0;
        }

        pr_verbose("device: %s: %s waits for %s, not in the tree yet", path, driver->name, provider_name);
    }
}

int device_wait_for(device_t* restrict device, device_t* restrict provider)
{
    kassert(device->priv.state == DEVICE_PROBING);
    kassert(!device_is_waiting(device));

    device_link_t* link = kmalloc(sizeof(device_link_t));

    if(link == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&device_tree_mutex);

    int retval = 0;

    if(relies_on_nolock(provider, device)) {
        retval = -EDEADLK;
    }
    else if(provider->priv.state == DEVICE_GONE) {
        retval = -ENODEV;
    }
    else if(provider->priv.state != DEVICE_BOUND) {
        device_incref(provider);
        device->priv.waits_for = provider;
        retval = -EAGAIN;
    }
    else {
        device_incref(provider);
        link->consumer = device;
        link->provider = provider;
        link->next_of_consumer = device->priv.providers;
        link->next_of_provider = provider->priv.consumers;
        device->priv.providers = link;
        provider->priv.consumers = link;
        link = nullptr;
    }

    mutex_release(&device_tree_mutex);

    kfree(link);

    return retval;
}
