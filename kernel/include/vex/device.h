#pragma once

#include <vex/compiler.h>
#include <vex/fwnode.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/types.h>

typedef struct bus_ops_s bus_ops_t;
typedef struct device_irq_s device_irq_t;
typedef struct device_link_s device_link_t;
typedef struct device_mapping_s device_mapping_t;
typedef struct driver_s driver_t;

#define DEVICE_NAME_MAX 32

enum device_state_e {
    DEVICE_PRESENT,   // In the tree, without a driver
    DEVICE_PROBING,   // Drivers are scored, and the best attaches
    DEVICE_WAITING,   // The winner waits for a provider to be bound, then the node is probed again
    DEVICE_BOUND,     // A driver has it
    DEVICE_DETACHING, // The driver lets go, then the resources go
    DEVICE_GONE,      // Out of the tree, until the last reference
};
typedef enum device_state_e device_state_t;

typedef struct device_private_s device_private_t;
typedef struct device_s device_t;

struct device_private_s {
    device_t* next; // Among the parent's children
    device_t* children;
    fwnode_resource_t* resources; // Of the firmware node, without the windows it passes on
    size_t num_resources;
    device_irq_t* irqs;         // Requested through device_request_irq
    device_mapping_t* mappings; // Made through device_map_io
    device_link_t* providers;   // What the driver uses, from attach until it lets go
    device_link_t* consumers;   // Whose drivers use the node
    device_t* waits_for;        // With a reference, while waiting for a node of the tree
    fwnode_t waits_for_fwnode;  // While waiting for a device the firmware describes but the tree doesn't have yet
    device_t* waiting_next;     // Among the nodes that wait
    device_t* worker_next;      // In the queue of device_worker, which holds a reference meanwhile
    unsigned pending;           // What the node is queued for, under the queue's lock
    unsigned refcount;
    device_state_t state;    // Changed by device_worker alone
    bool unplaced;           // Placed by the firmware rather than by the parent's bus, which ignores it
    bool fw_children_placed; // Until the node is unbound, which takes its children away
    bool gone;               // The hardware went away without warning
};

// A node of the device tree: one piece of hardware wherever it sits, with the
// bus it hangs from translating its resources and interrupts for the CPU
struct device_s {
    device_private_t priv;
    device_t* parent; // nullptr only on the root
    fwnode_t fwnode;  // ops is nullptr without one
    const bus_ops_t* bus_ops;
    void* bus_data; // The parent bus's own data about the node
    const driver_t* driver;
    void* ctxdata; // Of what drives the node
    char name[DEVICE_NAME_MAX];
};

/// Gets the root of the tree, made on the first call from the firmware's root node when there is one
/// @retval nullptr Out of memory
device_t* device_get_root(void) __nodiscard;

/// Adds a node below a parent, with the firmware's description of it
/// @param fwnode nullptr when the firmware doesn't describe the node
/// @param name The last component of the node's path, such as pci:0000:00:1f.0 or acpi:SIO
/// @param device Gets a new reference to the node, beside the one the tree holds
/// @retval -ENOMEM Out of memory
int device_add(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name, device_t** restrict device);

/// Queues the node to be bound to the driver with the highest score if it has none, to have its
/// firmware children placed if they aren't, and the same for every node below
/// @note Any context; device_worker does the work
void device_rescan(device_t* restrict device);

/// Queues the node to have its children removed and its driver detached; it stays in the tree
/// @note Any context; device_worker does the work
void device_unbind(device_t* restrict device);

/// Queues the node to be unbound and taken out of the tree
/// @note Any context; device_worker does the work
void device_remove(device_t* restrict device);

/// Makes the node's driver use a provider beyond its parent, so that the provider is unbound only after the node
/// @pre Called from the attach of the node's driver
/// @retval -EAGAIN The provider isn't bound yet; attach undoes what it did and returns this
/// @retval -EDEADLK The provider relies on the node, through the tree or through what it uses
/// @retval -ENODEV The provider is out of the tree
/// @retval -ENOMEM Out of memory
int device_wait_for(device_t* restrict device, device_t* restrict provider) __nodiscard;

/// Marks the node and all below it as gone from the hardware, so drivers stop waiting for answers from it
/// @note Doesn't remove the nodes; the bus that lost them asks for that
void device_set_gone(device_t* restrict device);

bool device_is_gone(const device_t* restrict device) __nodiscard;

void device_incref(device_t* restrict device);

/// @note Frees the node with the last reference, which goes with the tree's own only once the node is out of the tree
void device_decref(device_t* restrict device);

/// Writes the node's path from the root, such as acpi:PCI0/pci:0000:00:1f.0/isa:3f8
/// @return The path's length, as snprintf gives it
size_t device_get_path(const device_t* restrict device, char* restrict buffer, size_t size);

/// Maps a register range of the device, which the framework unmaps when the driver lets go of it
/// @param index Counts the device's port and memory ranges together, in its bus's order
/// @retval -ENODEV The device has no such range
/// @retval -ERANGE A bridge on the way doesn't pass the range on
/// @retval -ENOMEM No address space to map memory into
int device_map_io(device_t* restrict device, unsigned index, io_region_t* restrict region);

/// device_map_io with a memory type other than uncached, such as VPROT_CACHE_WC for video memory
/// @param index Counts the device's port and memory ranges together, in its bus's order
/// @retval -ENODEV The device has no such range
/// @retval -ERANGE A bridge on the way doesn't pass the range on
/// @retval -ENOMEM No address space to map memory into
/// @note x86 leaves two mappings of the same memory with different types undefined (PAT), so a driver maps a range once
int device_map_io_vprot(device_t* restrict device, unsigned index, unsigned vprot, io_region_t* restrict region);

void device_unmap_io(device_t* restrict device, io_region_t* restrict region);

/// Gets where the CPU sees a memory range of the device, without mapping it, as for mapping it to userspace
/// @param index Counts the device's port and memory ranges together, in its bus's order
/// @retval -ENODEV The device has no such range
/// @retval -ERANGE A bridge on the way doesn't pass the range on
/// @retval -EINVAL The CPU reaches the range through ports
int device_get_io_address(device_t* restrict device, unsigned index, uint64_t* restrict address);

/// Routes an interrupt of the device to a handler, sharing its GSI when the bus says it may
/// be shared; the framework releases it when the driver lets go of the device
/// @param index Counts the device's interrupts, in its bus's order
/// @retval -ENODEV The device has no such interrupt, or no interrupt controller has its GSI
/// @retval -EBUSY The interrupt is already requested, or its GSI is taken by a handler that doesn't share it
int device_request_irq(device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata);

void device_release_irq(device_t* restrict device, unsigned index);
