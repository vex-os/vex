#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct device_s device_t;

// A driver's probe returns one of these, or one above to win over
// others of the same level; a gap of 100 leaves room for that
#define DRIVER_SCORE_GENERIC  100 // Works with any device of its kind, through no more than the standard
#define DRIVER_SCORE_LOW      200 // Matches, but gives way to any other driver that does
#define DRIVER_SCORE_DEFAULT  300 // Drives a standard interface, such as a PCI class or a firmware id
#define DRIVER_SCORE_VENDOR   400 // Knows the vendor's own interface of the device
#define DRIVER_SCORE_SPECIFIC 500 // Knows the device by its exact id

enum {
    DRIVER_MATCH_END,
    DRIVER_MATCH_FIRMWARE,  // firmware_id, as fwnode_matches matches it
    DRIVER_MATCH_PCI_ID,    // vendor_id and device_id
    DRIVER_MATCH_PCI_CLASS, // class_id and subclass_id
};

typedef struct driver_match_s driver_match_t;
typedef struct driver_ops_s driver_ops_t;
typedef struct driver_s driver_t;

struct driver_match_s {
    unsigned kind; // DRIVER_MATCH_*
    const char* firmware_id;
    unsigned vendor_id;
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
};

struct driver_ops_s {
    /// Scores how well the driver suits a device, without touching its state
    /// @param match The first entry of the driver's table that the device matches
    /// @return A DRIVER_SCORE_* level or one above it, never zero
    /// @retval -ENODEV The driver doesn't take the device after all
    int (*probe)(device_t* restrict device, const driver_match_t* restrict match);

    /// Takes the device the driver won
    /// @retval -EAGAIN device_wait_for said to wait, and the framework probes the device again once the provider is bound
    /// @note Undoes what it did before it fails, as detach isn't called then
    int (*attach)(device_t* restrict device, const driver_match_t* restrict match);

    /// Readies the device to be detached, keeping it working, or refuses to release it
    /// @pre Called on everything that goes with a node asked to be unbound or removed, below and consumers first
    /// @retval -EBUSY The device can't go, which the driver logs with its reason; the whole request fails
    /// @note Optional: without it, the device can always go. A driver with offline also has online
    /// Not called on a device that is gone
    int (*offline)(device_t* restrict device);

    /// Undoes offline, when something else that had to go with the node refused
    /// @note Must not wait for the hardware to answer when device_is_gone
    void (*online)(device_t* restrict device);

    /// Takes the device's interfaces away and stops the hardware raising interrupts
    /// @post Calls through the interfaces fail. The interrupt handlers can still run, but start nothing new
    /// The framework then takes the resources back
    /// @note Must not wait for the hardware to answer when device_is_gone
    void (*detach)(device_t* restrict device);

    /// Frees what the driver keeps in ctxdata, nullptr included
    /// @pre Called after detach, or after attach failed, once the interrupt handlers can't run
    void (*release)(device_t* restrict device);
};

struct driver_s {
    const char* name;
    const driver_ops_t* ops;
    const driver_match_t* matches; // Ends with DRIVER_MATCH_END
};

#define DRIVER_MATCH_FIRMWARE(dm_id)   \
    {                                  \
        .kind = DRIVER_MATCH_FIRMWARE, \
        .firmware_id = (dm_id),        \
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
