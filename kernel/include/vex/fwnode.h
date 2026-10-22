#pragma once

#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/types.h>

enum {
    FWNODE_RESOURCE_PORT,
    FWNODE_RESOURCE_MEMORY,
    FWNODE_RESOURCE_BUS, // Bus numbers, such as those under a PCI root bridge
    FWNODE_RESOURCE_IRQ,
    FWNODE_RESOURCE_DMA,
};

#define FWNODE_RANGE_WINDOW     0x01U // The node passes the range on to its children, as a bridge does
#define FWNODE_RANGE_OTHER_TYPE 0x02U // The parent sees the range in the other space: ports in memory, or memory in ports

#define FWNODE_IRQ_ISA    0x01U // An ISA IRQ, which the ISA bus maps to a GSI; a GSI otherwise
#define FWNODE_IRQ_SHARED 0x02U

typedef struct fwnode_range_s fwnode_range_t;
typedef struct fwnode_irq_s fwnode_irq_t;
typedef struct fwnode_resource_s fwnode_resource_t;
typedef struct fwnode_s fwnode_t;
typedef struct fwnode_fixed_s fwnode_fixed_t;
typedef struct fwnode_ops_s fwnode_ops_t;

struct fwnode_range_s {
    uint64_t start;
    uint64_t length;
    uint64_t translation; // Added to start, gives where the parent sees the range
    unsigned flags;       // FWNODE_RANGE_*
};

struct fwnode_irq_s {
    uint32_t number;
    unsigned trigger;  // TRIGGER_*
    unsigned polarity; // ACTIVE_*
    unsigned flags;    // FWNODE_IRQ_*
};

struct fwnode_resource_s {
    unsigned type; // FWNODE_RESOURCE_*
    union {
        fwnode_range_t range; // A port, memory or bus range
        fwnode_irq_t irq;
        uint8_t dma_channel;
    };
};

struct fwnode_ops_s {
    /// @return Whether the device is there and decodes its resources
    bool (*is_present)(const fwnode_t* restrict node);

    /// @param ids Ends with nullptr; ACPI matches _HID, then _CID
    bool (*matches)(const fwnode_t* restrict node, const char* const* restrict ids);

    /// Gets the node's address on its parent's bus, such as a PCI device and function in ACPI's _ADR
    /// @retval -ENOENT The node has none
    int (*get_address)(const fwnode_t* restrict node, uint64_t* restrict address);

    /// Gets the hardware id of the processor the node describes, such as its APIC ID on x86
    /// @retval -ENOENT The node isn't a processor, or the firmware doesn't say which one it is
    int (*get_cpu_id)(const fwnode_t* restrict node, uint64_t* restrict cpu_id);

    /// Gets the node's first id, for names and messages
    /// @retval -ENOENT The node has none
    int (*get_id)(const fwnode_t* restrict node, char* restrict buffer, size_t size);

    /// Gets the node's name as a component of a device path, such as acpi:SIO for an ACPI name segment
    /// @retval -ENOENT The node has none
    /// @retval -ERANGE The buffer is too small for the name
    int (*get_name)(const fwnode_t* restrict node, char* restrict buffer, size_t size);

    /// @note Includes the children the firmware says are absent, which fwnode_is_present tells apart
    void (*for_each_child)(const fwnode_t* restrict node, void (*callback)(const fwnode_t* child, void* arg), void* arg);

    /// @retval -ENOENT The node has no resources
    int (*for_each_resource)(const fwnode_t* restrict node, void (*callback)(const fwnode_resource_t* resource, void* arg), void* arg);

    /// Gets the interrupt that an INTx pin of a device on the PCI bus below the node raises, as ACPI's _PRT gives it
    /// @param slot The device number on that bus
    /// @param pin 0 for INTA through 3 for INTD
    /// @retval -ENOENT The node doesn't route the pin, so it swizzles on through the bridge above
    /// @retval -ENXIO The pin goes to an interrupt link that has no interrupt yet
    int (*route_pci_interrupt)(const fwnode_t* restrict node, unsigned slot, unsigned pin, fwnode_irq_t* restrict irq);

    /// Calls back for each device the node needs working before it can be driven, as ACPI's _DEP lists them
    /// @retval -ENOENT The node needs none
    int (*for_each_dependency)(const fwnode_t* restrict node, void (*callback)(const fwnode_t* provider, void* arg), void* arg);

    /// Has the firmware take the device out once the tree released it, or tells the firmware it can't go
    /// @param status 0 when the node is out of the tree, or -EBUSY when a driver refused to release it
    /// @note Does nothing where the firmware has no say in it
    void (*eject)(const fwnode_t* restrict node, int status);
};

// A device as the firmware describes it, held by value; the
// callbacks get it on the stack, so they copy what they keep
struct fwnode_s {
    const fwnode_ops_t* ops;
    void* handle; // The firmware's own, such as a uACPI namespace node
};

// A device that code knows is there, such as legacy hardware that a probe
// found where the firmware says nothing; it has no children or dependencies
struct fwnode_fixed_s {
    const char* name; // A device path names the node fixed:<name>
    const char* id;   // Matched as ACPI matches _HID, such as PNP0100
    const fwnode_resource_t* resources;
    size_t num_resources;
};

/// Gets the node the devices of the system hang from
/// @retval -ENODEV The system has no firmware tables to describe its devices
int fwnode_root(fwnode_t* restrict root);

/// Makes a node of a fixed description, which has to outlive every copy of the node
void fwnode_init_fixed(fwnode_t* restrict node, const fwnode_fixed_t* restrict fixed);

/// Calls back for each node that a fixed= option of the command line puts under the node with the path
/// @note The nodes are fixed ones, and their descriptions live as long as the kernel
void fwnode_for_each_fixed_option(const char* restrict parent_path, void (*callback)(const fwnode_t* child, void* arg), void* arg);

/// @return Whether two nodes are the same device of the firmware
static __always_inline bool fwnode_is_same(const fwnode_t* first, const fwnode_t* second)
{
    return first->ops == second->ops && first->handle == second->handle;
}

static __always_inline bool fwnode_is_present(const fwnode_t* restrict node)
{
    return node->ops->is_present(node);
}

static __always_inline bool fwnode_matches(const fwnode_t* restrict node, const char* const* restrict ids)
{
    return node->ops->matches(node, ids);
}

static __always_inline int fwnode_get_address(const fwnode_t* restrict node, uint64_t* restrict address)
{
    return node->ops->get_address(node, address);
}

static __always_inline int fwnode_get_cpu_id(const fwnode_t* restrict node, uint64_t* restrict cpu_id)
{
    return node->ops->get_cpu_id(node, cpu_id);
}

static __always_inline int fwnode_get_id(const fwnode_t* restrict node, char* restrict buffer, size_t size)
{
    return node->ops->get_id(node, buffer, size);
}

static __always_inline int fwnode_get_name(const fwnode_t* restrict node, char* restrict buffer, size_t size)
{
    return node->ops->get_name(node, buffer, size);
}

static __always_inline void fwnode_for_each_child(const fwnode_t* restrict node, void (*callback)(const fwnode_t* child, void* arg),
    void* arg)
{
    node->ops->for_each_child(node, callback, arg);
}

static __always_inline int fwnode_for_each_resource(const fwnode_t* restrict node,
    void (*callback)(const fwnode_resource_t* resource, void* arg), void* arg)
{
    return node->ops->for_each_resource(node, callback, arg);
}

static __always_inline int fwnode_route_pci_interrupt(const fwnode_t* restrict node, unsigned slot, unsigned pin,
    fwnode_irq_t* restrict irq)
{
    return node->ops->route_pci_interrupt(node, slot, pin, irq);
}

static __always_inline void fwnode_eject(const fwnode_t* restrict node, int status)
{
    node->ops->eject(node, status);
}

static __always_inline int fwnode_for_each_dependency(const fwnode_t* restrict node, void (*callback)(const fwnode_t* provider, void* arg),
    void* arg)
{
    return node->ops->for_each_dependency(node, callback, arg);
}
