#pragma once

#include <vex/compiler.h>
#include <vex/fwnode.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/types.h>

typedef struct device_s device_t;
typedef struct driver_match_s driver_match_t;
typedef struct interrupt_action_s interrupt_action_t;

typedef struct bus_ops_s bus_ops_t;

// What a node does for its children; a missing op, or a missing table,
// falls back to what the firmware describes and passes ranges on as they are
struct bus_ops_s {
    /// Places a present firmware child of the bus on the bus
    /// @retval 0 The bus placed the child, or knows it as one of its own that isn't there
    /// @retval -ENOENT The bus can't place the child, so it becomes a firmware child of the node
    int (*place_child)(device_t* restrict bus, const fwnode_t* restrict child);

    /// Looks for the children the bus finds on its own, adding new ones and removing those that went away
    /// @pre Called on device_worker
    /// @note A child the bus has rescan for is the bus's to judge, whatever its firmware node says;
    /// a lost one goes through device_set_gone and device_remove
    void (*rescan)(device_t* restrict bus);

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

    /// Routes an interrupt of a child without an interrupt controller, as PCI does with MSI
    /// @param index As device_request_irq counts it
    /// @retval -ENOENT The bus doesn't route this interrupt. get_irq gives it instead
    /// @retval -ENOMEM Out of memory, or no vector is free
    /// @note Detaching the action releases the interrupt. enable_irq and disable_irq don't apply to it
    int (*attach_irq)(device_t* restrict bus, device_t* restrict child, unsigned index, irq_handler_t handler, void* restrict ctxdata,
        interrupt_action_t** restrict action);

    /// Lets an interrupt of a child through once a handler is attached to it, as PCI does by clearing Interrupt Disable
    /// @param index As device_request_irq counts it
    void (*enable_irq)(device_t* restrict bus, device_t* restrict child, unsigned index);

    /// Stops an interrupt of a child before its handler goes
    /// @param index As device_request_irq counts it
    void (*disable_irq)(device_t* restrict bus, device_t* restrict child, unsigned index);

    /// Takes back what the bus gave a child beyond its interrupts and device_map_io, such as MSI and bus mastering
    /// @pre The child's driver has released the child
    void (*release_child)(device_t* restrict bus, device_t* restrict child);

    /// Forgets a child on its way out of the tree, with what the bus keeps about it in bus_data
    /// @pre The child has no children and no driver
    void (*remove_child)(device_t* restrict bus, device_t* restrict child);

    /// Translates a range of this bus to the bus above it
    /// @param type FWNODE_RESOURCE_PORT or FWNODE_RESOURCE_MEMORY, changed when the bus above sees the range in the other space
    /// @retval -ERANGE The node doesn't pass the range on
    int (*translate_range)(device_t* restrict bus, unsigned* restrict type, uint64_t* restrict address, uint64_t length);
};

/// Translates a range of a bus up to the CPU
/// @param type In: FWNODE_RESOURCE_PORT or FWNODE_RESOURCE_MEMORY as on the bus; out: how the CPU reaches the range
/// @retval -ERANGE A bridge on the way doesn't pass the range on
int device_translate_range(device_t* restrict bus, unsigned* restrict type, uint64_t* restrict address, uint64_t length);

/// Maps a range of a bus for the CPU, as uncached memory or as ports
/// @param type FWNODE_RESOURCE_PORT or FWNODE_RESOURCE_MEMORY, as on the bus
/// @retval -ERANGE A bridge on the way doesn't pass the range on
/// @retval -ENOMEM No address space to map memory into
int device_map_range(device_t* restrict bus, unsigned type, uint64_t start, uint64_t length, io_region_t* restrict region);

void device_unmap_range(io_region_t* restrict region);
