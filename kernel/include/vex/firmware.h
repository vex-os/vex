#pragma once

#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/types.h>

enum {
    FIRMWARE_RESOURCE_PORT,
    FIRMWARE_RESOURCE_MEMORY,
    FIRMWARE_RESOURCE_BUS, // Bus numbers, such as those under a PCI root bridge
    FIRMWARE_RESOURCE_IRQ,
    FIRMWARE_RESOURCE_DMA,
};

#define FIRMWARE_RANGE_WINDOW     0x01U // The node passes the range on to its children, as a bridge does
#define FIRMWARE_RANGE_OTHER_TYPE 0x02U // The parent sees the range in the other space: ports in memory, or memory in ports

#define FIRMWARE_IRQ_ISA    0x01U // An ISA IRQ, which the ISA bus maps to a GSI; a GSI otherwise
#define FIRMWARE_IRQ_SHARED 0x02U

typedef struct firmware_range_s firmware_range_t;
typedef struct firmware_irq_s firmware_irq_t;
typedef struct firmware_resource_s firmware_resource_t;
typedef struct firmware_node_s firmware_node_t;
typedef struct firmware_node_ops_s firmware_node_ops_t;

struct firmware_range_s {
    uint64_t start;
    uint64_t length;
    uint64_t translation; // Added to start, gives where the parent sees the range
    unsigned flags;       // FIRMWARE_RANGE_*
};

struct firmware_irq_s {
    uint32_t number;
    unsigned trigger;  // TRIGGER_*
    unsigned polarity; // ACTIVE_*
    unsigned flags;    // FIRMWARE_IRQ_*
};

struct firmware_resource_s {
    unsigned type; // FIRMWARE_RESOURCE_*
    union {
        firmware_range_t range; // A port, memory or bus range
        firmware_irq_t irq;
        uint8_t dma_channel;
    };
};

struct firmware_node_ops_s {
    /// @return Whether the device is there and decodes its resources
    bool (*is_present)(const firmware_node_t* restrict node);

    /// @param ids Ends with nullptr; ACPI matches _HID, then _CID
    bool (*matches)(const firmware_node_t* restrict node, const char* const* restrict ids);

    /// Gets the node's address on its parent's bus, such as a PCI device and function in ACPI's _ADR
    /// @retval -ENOENT The node has none
    int (*get_address)(const firmware_node_t* restrict node, uint64_t* restrict address);

    /// Gets the node's first id, for names and messages
    /// @retval -ENOENT The node has none
    int (*get_id)(const firmware_node_t* restrict node, char* restrict buffer, size_t size);

    /// @note Skips the children the firmware says are absent, with all below them
    void (*for_each_child)(const firmware_node_t* restrict node, void (*callback)(const firmware_node_t* child, void* arg), void* arg);

    /// @retval -ENOENT The node has no resources
    int (*for_each_resource)(const firmware_node_t* restrict node, void (*callback)(const firmware_resource_t* resource, void* arg),
        void* arg);
};

// A device as the firmware describes it, held by value; the
// callbacks get it on the stack, so they copy what they keep
struct firmware_node_s {
    const firmware_node_ops_t* ops;
    void* handle; // The firmware's own, such as a uACPI namespace node
};

/// Gets the node the devices of the system hang from
/// @retval -ENODEV The system has no firmware tables to describe its devices
int firmware_root(firmware_node_t* restrict root);

static __always_inline bool firmware_node_is_present(const firmware_node_t* restrict node)
{
    return node->ops->is_present(node);
}

static __always_inline bool firmware_node_matches(const firmware_node_t* restrict node, const char* const* restrict ids)
{
    return node->ops->matches(node, ids);
}

static __always_inline int firmware_node_get_address(const firmware_node_t* restrict node, uint64_t* restrict address)
{
    return node->ops->get_address(node, address);
}

static __always_inline int firmware_node_get_id(const firmware_node_t* restrict node, char* restrict buffer, size_t size)
{
    return node->ops->get_id(node, buffer, size);
}

static __always_inline void firmware_node_for_each_child(const firmware_node_t* restrict node,
    void (*callback)(const firmware_node_t* child, void* arg), void* arg)
{
    node->ops->for_each_child(node, callback, arg);
}

static __always_inline int firmware_node_for_each_resource(const firmware_node_t* restrict node,
    void (*callback)(const firmware_resource_t* resource, void* arg), void* arg)
{
    return node->ops->for_each_resource(node, callback, arg);
}
