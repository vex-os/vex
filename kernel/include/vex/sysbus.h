#pragma once

#include <vex/compiler.h>
#include <vex/fwnode.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/types.h>

typedef struct device_s device_t;

typedef struct sysbus_device_private_s sysbus_device_private_t;
typedef struct sysbus_device_s sysbus_device_t;
typedef struct sysbus_driver_ops_s sysbus_driver_ops_t;
typedef struct sysbus_driver_private_s sysbus_driver_private_t;
typedef struct sysbus_driver_s sysbus_driver_t;

struct sysbus_device_private_s {
    sysbus_device_t* next;
    sysbus_driver_t* driver;
    device_t* device;
};

// A firmware child of a node, which the node's bus
// can't place, on any level of the tree
struct sysbus_device_s {
    sysbus_device_private_t priv;
    const char* name; // The node's first id
    void* ctxdata;
};

struct sysbus_driver_ops_s {
    int (*probe)(sysbus_device_t* restrict device);
    void (*remove)(sysbus_device_t* restrict device);
};

struct sysbus_driver_private_s {
    sysbus_driver_t* next;
};

struct sysbus_driver_s {
    sysbus_driver_private_t priv;
    const sysbus_driver_ops_t* ops;
    const char* const* ids; // Ends with nullptr; matched as fwnode_matches does
};

/// Binds the drivers that match a new firmware child to it
/// @pre device_enumerate calls it, once for each node it makes from a firmware child
void sysbus_device_add(device_t* restrict device);

int sysbus_driver_add(sysbus_driver_t* restrict driver);
int sysbus_driver_remove(sysbus_driver_t* restrict driver);

/// Gets the firmware's node for the device
void sysbus_get_node(const sysbus_device_t* restrict device, fwnode_t* restrict node);

/// Maps a register range of the device, counting port and memory ranges together in the firmware's order
/// @retval -ENODEV The device has no such range
int sysbus_map_io(sysbus_device_t* restrict device, unsigned index, io_region_t* restrict region);

void sysbus_unmap_io(io_region_t* restrict region);

/// Routes an interrupt of the device to a handler, sharing its GSI when the firmware says it may be shared
/// @param index Counts the device's interrupts in the firmware's order
/// @retval -ENODEV The device has no such interrupt, or no interrupt controller has its GSI
/// @retval -EBUSY The interrupt is already requested, or its GSI is taken by a handler that doesn't share it
int sysbus_request_irq(sysbus_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata);

void sysbus_release_irq(sysbus_device_t* restrict device, unsigned index);
