#pragma once

#include <vex/compiler.h>
#include <vex/fwnode.h>
#include <vex/intctl.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/types.h>

typedef struct isa_bus_s isa_bus_t;

typedef struct isa_device_private_s isa_device_private_t;
typedef struct isa_device_s isa_device_t;
typedef struct isa_driver_ops_s isa_driver_ops_t;
typedef struct isa_driver_private_s isa_driver_private_t;
typedef struct isa_driver_s isa_driver_t;

struct isa_device_private_s {
    isa_device_t* next;
    isa_bus_t* bus;
    isa_driver_t* driver;
    fwnode_t node;
    fwnode_resource_t* resources;
    interrupt_action_t** interrupts; // Parallel to resources, set on the IRQs requested
    size_t num_resources;
};

// A device behind an ISA bridge, with ports and memory that are addresses on the bus above it
struct isa_device_s {
    isa_device_private_t priv;
    const char* name; // The node's first id
    void* ctxdata;
};

struct isa_driver_ops_s {
    int (*probe)(isa_device_t* restrict device);
    void (*remove)(isa_device_t* restrict device);
};

struct isa_driver_private_s {
    isa_driver_t* next;
};

struct isa_driver_s {
    isa_driver_private_t priv;
    const isa_driver_ops_t* ops;
    const char* const* ids; // Ends with nullptr; matched as fwnode_matches does
};

int isa_driver_add(isa_driver_t* restrict driver);
int isa_driver_remove(isa_driver_t* restrict driver);

/// Gets the firmware's node for the device
void isa_get_node(const isa_device_t* restrict device, fwnode_t* restrict node);

/// Maps a register range of the device, counting port and memory ranges together in the firmware's order
/// @retval -ENODEV The device has no such range
/// @retval -ERANGE The bridges above don't pass the range on
int isa_map_io(isa_device_t* restrict device, unsigned index, io_region_t* restrict region);

void isa_unmap_io(io_region_t* restrict region);

/// Routes an interrupt of the device to a handler, sharing its GSI when the firmware says it may be shared
/// @param index Counts the device's interrupts in the firmware's order
/// @retval -ENODEV The device has no such interrupt, or no interrupt controller has its GSI
/// @retval -EBUSY The interrupt is already requested, or its GSI is taken by a handler that doesn't share it
int isa_request_irq(isa_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata);

void isa_release_irq(isa_device_t* restrict device, unsigned index);

/// Maps an ISA IRQ to its GSI through the MADT's overrides; without one the GSI is the IRQ itself, edge-triggered and active-high
void isa_irq_route(uint8_t line, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity);
