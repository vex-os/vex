#pragma once

#include <vex/compiler.h>
#include <vex/fwnode.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/types.h>

typedef struct device_irq_s device_irq_t;

#define DEVICE_NAME_MAX 32

// A driver bids one of these, or one above to win over others
// of the same level; a gap of 100 leaves room for that
#define DRIVER_BID_GENERIC  100 // Works with any device of its kind, through no more than the standard
#define DRIVER_BID_LOW      200 // Matches, but gives way to any other driver that does
#define DRIVER_BID_DEFAULT  300 // Drives a standard interface, such as a PCI class or a firmware id
#define DRIVER_BID_VENDOR   400 // Knows the vendor's own interface of the device
#define DRIVER_BID_SPECIFIC 500 // Knows the device by its exact id

enum {
    DRIVER_MATCH_END,
    DRIVER_MATCH_FW,        // fw_id, as fwnode_matches matches it
    DRIVER_MATCH_PCI_ID,    // vendor_id and device_id
    DRIVER_MATCH_PCI_CLASS, // class_id and subclass_id
};

typedef struct device_bus_ops_s device_bus_ops_t;
typedef struct device_private_s device_private_t;
typedef struct device_s device_t;
typedef struct driver_match_s driver_match_t;
typedef struct driver_ops_s driver_ops_t;
typedef struct driver_s driver_t;

// What a node does for its children; a missing op, or a missing table,
// falls back to what the firmware describes and passes ranges on as they are
struct device_bus_ops_s {
    /// Places a present firmware child of the bus on the bus
    /// @retval 0 The bus placed the child, or knows it as one of its own that isn't there
    /// @retval -ENOENT The bus can't place the child, so it becomes a firmware child of the node
    int (*place_child)(device_t* restrict bus, const fwnode_t* restrict child);

    /// @return Whether a child matches an entry of a driver's table that is of the bus's own kinds
    bool (*match)(device_t* restrict bus, device_t* restrict child, const driver_match_t* restrict match);

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
    bool fw_child; // Placed by the firmware rather than by the parent's bus, which ignores it
};

// A node of the device tree: one piece of hardware wherever it sits, with the
// bus it hangs from translating its resources and interrupts for the CPU
struct device_s {
    device_private_t priv;
    device_t* parent; // nullptr only on the root
    fwnode_t fwnode;  // ops is nullptr without one
    const device_bus_ops_t* bus_ops;
    void* bus_ctxdata; // The parent bus's own data about the node
    const driver_t* driver;
    void* ctxdata; // Of what drives the node
    char name[DEVICE_NAME_MAX];
};

struct driver_match_s {
    unsigned kind; // DRIVER_MATCH_*
    const char* fw_id;
    unsigned vendor_id;
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
};

struct driver_ops_s {
    /// Bids for a device, without touching its state
    /// @param match The first entry of the driver's table that the device matches
    /// @param bid A DRIVER_BID_* level, or one above it
    /// @retval -ENODEV The driver doesn't take the device after all
    int (*probe)(device_t* restrict device, const driver_match_t* restrict match, unsigned* restrict bid);

    /// Takes the device the driver won
    /// @note Undoes what it did before it fails, as detach isn't called then
    int (*attach)(device_t* restrict device, const driver_match_t* restrict match);

    /// Lets go of the device
    /// @note nullptr on a driver that can't let go yet
    void (*detach)(device_t* restrict device);
};

struct driver_s {
    const char* name;
    const driver_ops_t* ops;
    const driver_match_t* matches; // Ends with DRIVER_MATCH_END
};

#define DRIVER_MATCH_FW(dm_id)   \
    {                            \
        .kind = DRIVER_MATCH_FW, \
        .fw_id = (dm_id),        \
    }

#define DRIVER_MATCH_PCI_ID(dm_vendor, dm_device) \
    {                                             \
        .kind = DRIVER_MATCH_PCI_ID,              \
        .vendor_id = (dm_vendor),                 \
        .device_id = (dm_device),                 \
    }

#define DRIVER_MATCH_PCI_CLASS(dm_class, dm_subclass) \
    {                                                 \
        .kind = DRIVER_MATCH_PCI_CLASS,               \
        .class_id = (dm_class),                       \
        .subclass_id = (dm_subclass),                 \
    }

#define DRIVER_MATCH_END()        \
    {                             \
        .kind = DRIVER_MATCH_END, \
    }

#define define_driver(dr_name, dr_ops, dr_matches)                            \
    static const __used __section(".drivers") driver_t __driver_##dr_name = { \
        .name = #dr_name,                                                     \
        .ops = (dr_ops),                                                      \
        .matches = (dr_matches),                                              \
    }

/// Gets the root of the tree, made on the first call from the firmware's root node when there is one
/// @retval nullptr Out of memory
device_t* device_get_root(void) __nodiscard;

/// Adds a node below a parent, with the firmware's description of it
/// @param fwnode nullptr when the firmware doesn't describe the node
/// @param name The last component of the node's path, such as pci:0000:00:1f.0 or acpi:SIO
/// @param device Gets a new reference to the node, beside the one the tree holds
/// @retval -ENOMEM Out of memory
int device_add(device_t* restrict parent, const fwnode_t* restrict fwnode, const char* restrict name, device_t** restrict device);

/// Binds the node to the driver with the highest bid, then places its firmware children, then
/// does the same below; the firmware children the node's bus can't place become nodes of
/// their own named by the firmware
/// @note Goes through a node only once, as nothing takes nodes out of the tree yet
void device_enumerate(device_t* restrict device);

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
