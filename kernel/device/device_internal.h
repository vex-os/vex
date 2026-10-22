#pragma once

#include <vex/device.h>
#include <vex/device_bus.h>
#include <vex/mutex.h>

#define DEVICE_PATH_MAX 256

#define DEVICE_REQUEST_RESCAN 0x01U
#define DEVICE_REQUEST_UNBIND 0x02U
#define DEVICE_REQUEST_REMOVE 0x04U

struct device_link_s {
    device_t* consumer;
    device_t* provider;
    device_link_t* next_of_consumer;
    device_link_t* next_of_provider;
};

// Guards the links between nodes
// and the interrupts requested on them
extern mutex_t device_tree_mutex;

/// @return A new reference to the next child after prev, or the first one when prev is nullptr
/// @pre The caller holds a reference to prev, which keeps it in the list
device_t* device_next_child(device_t* parent, device_t* prev) __nodiscard;

device_t* device_find_by_fwnode(const fwnode_t* restrict fwnode) __nodiscard;

/// Adds a node for each present child of the node's firmware node that its bus doesn't place
void device_place_firmware_children(device_t* restrict device);

/// @note Any context; device_worker does the work
void device_queue_request(device_t* restrict device, unsigned request);

/// Takes back what the device got through the framework and from its bus
/// @pre The driver has let go of the device
void device_release_resources(device_t* restrict device);

bool device_is_waiting(const device_t* restrict device) __nodiscard;

/// Waits for what the firmware says the device needs, as device_wait_for does
/// @pre The device is being probed, on device_worker
int device_wait_for_dependencies(device_t* restrict device) __nodiscard;

/// @pre The device's driver has let go of it, or never attached
void device_unlink_providers(device_t* restrict device);

/// @pre The device is being probed and waits for a provider
void device_start_waiting(device_t* restrict device);

void device_stop_waiting(device_t* restrict device);

/// Queues a rescan of the nodes that wait for the provider
/// @note Called when the provider is bound or leaves the tree
void device_wake_waiters(device_t* restrict provider);

void device_log_wait(device_t* restrict device, const char* restrict path, const driver_t* restrict driver);

static __always_inline bool device_is_range(const fwnode_resource_t* restrict resource)
{
    return resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;
}

// The bus a node hangs from, or nullptr when the node only hangs
// from its parent as the firmware describes it, without a bus between
static __always_inline device_t* device_bus_of(device_t* restrict device)
{
    device_t* bus = device->parent;

    if(bus == nullptr || bus->bus_ops == nullptr || device->priv.unplaced) {
        return nullptr;
    }

    return bus;
}
