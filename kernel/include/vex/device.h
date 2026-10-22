#pragma once

#include <vex/compiler.h>
#include <vex/fwnode.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/types.h>

typedef struct device_irq_s device_irq_t;

#define DEVICE_NAME_MAX 32

typedef struct device_bus_ops_s device_bus_ops_t;
typedef struct device_private_s device_private_t;
typedef struct device_s device_t;

// What a node does for its children; a missing op, or a missing table,
// falls back to what the firmware describes and passes ranges on as they are
struct device_bus_ops_s {
    /// Gets a register range of a child, as an address on this bus
    /// @param index Counts the child's port and memory ranges together, in the bus's order
    /// @retval -ENODEV The child has no such range
    int (*get_range)(device_t* restrict bus, device_t* restrict child, unsigned index, fwnode_resource_t* restrict range);

    /// Gets an interrupt of a child as the interrupt controllers see it, or as an ISA IRQ
    /// @param index Counts the child's interrupts, in the bus's order
    /// @retval -ENODEV The child has no such interrupt, or nothing routes it
    int (*get_irq)(device_t* restrict bus, device_t* restrict child, unsigned index, fwnode_irq_t* restrict irq);

    /// Translates a range of this bus to the bus above it
    /// @param type FWNODE_RESOURCE_PORT or FWNODE_RESOURCE_MEMORY, changed when the bus above sees the range in the other space
    /// @retval -ERANGE The node doesn't pass the range on
    int (*translate_range)(device_t* restrict bus, unsigned* restrict type, uint64_t* restrict address, uint64_t length);
};

struct device_private_s {
    device_t* next; // Among the parent's children
    device_t* children;
    fwnode_resource_t* resources; // Of the firmware node, without the windows it passes on
    size_t num_resources;
    device_irq_t* irqs; // Requested through device_request_irq
    unsigned refcount;
};

// A node of the device tree: one piece of hardware wherever it sits, with the
// bus it hangs from translating its resources and interrupts for the CPU
struct device_s {
    device_private_t priv;
    device_t* parent; // nullptr only on the root
    fwnode_t fwnode;  // ops is nullptr without one
    const device_bus_ops_t* bus_ops;
    void* bus_ctxdata; // The parent bus's own data about the node
    void* ctxdata;     // Of what drives the node
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

void device_incref(device_t* restrict device);

/// @note Frees the node with the last reference, which goes with the tree's own only once the node is out of the tree
void device_decref(device_t* restrict device);

/// Writes the node's path from the root, such as acpi:PCI0/pci:0000:00:1f.0/isa:3f8
/// @return The path's length, as snprintf gives it
size_t device_get_path(const device_t* restrict device, char* restrict buffer, size_t size);

/// Translates a range of a bus up to the CPU
/// @param type In: FWNODE_RESOURCE_PORT or FWNODE_RESOURCE_MEMORY as on the bus; out: how the CPU reaches the range
/// @retval -ERANGE A bridge on the way doesn't pass the range on
int device_translate_range(device_t* restrict bus, unsigned* restrict type, uint64_t* restrict address, uint64_t length);

/// Maps a range of a bus for the CPU, as uncached memory or as ports
/// @param type FWNODE_RESOURCE_PORT or FWNODE_RESOURCE_MEMORY, as on the bus
/// @retval -ERANGE A bridge on the way doesn't pass the range on
/// @retval -ENOMEM No address space to map memory into
int device_map_range(device_t* restrict bus, unsigned type, uint64_t start, uint64_t length, io_region_t* restrict region);

/// Maps a register range of the device
/// @param index Counts the device's port and memory ranges together, in its bus's order
/// @retval -ENODEV The device has no such range
/// @retval -ERANGE A bridge on the way doesn't pass the range on
/// @retval -ENOMEM No address space to map memory into
int device_map_io(device_t* restrict device, unsigned index, io_region_t* restrict region);

void device_unmap_io(io_region_t* restrict region);

/// Routes an interrupt of the device to a handler, sharing its GSI when the bus says it may be shared
/// @param index Counts the device's interrupts, in its bus's order
/// @retval -ENODEV The device has no such interrupt, or no interrupt controller has its GSI
/// @retval -EBUSY The interrupt is already requested, or its GSI is taken by a handler that doesn't share it
int device_request_irq(device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata);

void device_release_irq(device_t* restrict device, unsigned index);
