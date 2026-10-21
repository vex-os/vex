#pragma once

#include <vex/compiler.h>
#include <vex/firmware.h>
#include <vex/intctl.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/types.h>

typedef struct platform_device_private_s platform_device_private_t;
typedef struct platform_device_s platform_device_t;
typedef struct platform_driver_ops_s platform_driver_ops_t;
typedef struct platform_driver_private_s platform_driver_private_t;
typedef struct platform_driver_s platform_driver_t;

struct platform_device_private_s {
    platform_device_t* next;
    platform_driver_t* driver;
    firmware_node_t node;
    firmware_resource_t* resources;
    interrupt_action_t** interrupts; // Parallel to resources, set on the IRQs requested
    size_t num_resources;
};

// A device the firmware describes on no bus of
// its own, with registers the CPU reaches directly
struct platform_device_s {
    platform_device_private_t priv;
    const char* name; // The node's first id
    void* ctxdata;
};

struct platform_driver_ops_s {
    int (*probe)(platform_device_t* restrict device);
    void (*remove)(platform_device_t* restrict device);
};

struct platform_driver_private_s {
    platform_driver_t* next;
};

struct platform_driver_s {
    platform_driver_private_t priv;
    const platform_driver_ops_t* ops;
    const char* const* ids; // Ends with nullptr; matched as firmware_node_matches does
};

int platform_driver_add(platform_driver_t* restrict driver);
int platform_driver_remove(platform_driver_t* restrict driver);

/// Gets the firmware's node for the device
void platform_get_node(const platform_device_t* restrict device, firmware_node_t* restrict node);

/// Maps a register range of the device, counting port and memory ranges together in the firmware's order
/// @retval -ENODEV The device has no such range
int platform_map_io(platform_device_t* restrict device, unsigned index, io_region_t* restrict region);

void platform_unmap_io(io_region_t* restrict region);

/// Routes an interrupt of the device to a handler, sharing its GSI when the firmware says it may be shared
/// @param index Counts the device's interrupts in the firmware's order
/// @retval -ENODEV The device has no such interrupt, or no interrupt controller has its GSI
/// @retval -EBUSY The interrupt is already requested, or its GSI is taken by a handler that doesn't share it
int platform_request_irq(platform_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata);

void platform_release_irq(platform_device_t* restrict device, unsigned index);
