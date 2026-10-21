#include <uacpi/namespace.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>
#include <vex/errno.h>
#include <vex/firmware.h>
#include <vex/intctl.h>
#include <vex/string.h>
#include <vex/strings.h>

// ACPI 6.5, 6.3.7 _STA
#define STA_PRESENT     0x01U
#define STA_ENABLED     0x02U
#define STA_FUNCTIONING 0x08U

typedef struct child_walk_s child_walk_t;
typedef struct resource_walk_s resource_walk_t;

struct child_walk_s {
    void (*callback)(const firmware_node_t* child, void* arg);
    void* arg;
};

struct resource_walk_s {
    void (*callback)(const firmware_resource_t* resource, void* arg);
    void* arg;
};

static const firmware_node_ops_t acpi_node_ops;

static uacpi_namespace_node* namespace_node_of(const firmware_node_t* restrict node)
{
    return node->handle;
}

static uint32_t status_of(uacpi_namespace_node* restrict namespace_node)
{
    uacpi_u32 status;

    if(uacpi_unlikely_error(uacpi_eval_sta(namespace_node, &status))) {
        return 0;
    }

    return status;
}

static bool acpi_node_is_present(const firmware_node_t* restrict node)
{
    uint32_t status = status_of(namespace_node_of(node));
    return (status & (STA_PRESENT | STA_ENABLED)) == (STA_PRESENT | STA_ENABLED);
}

static bool acpi_node_matches(const firmware_node_t* restrict node, const char* const* restrict ids)
{
    return uacpi_device_matches_pnp_id(namespace_node_of(node), ids);
}

static int acpi_node_get_address(const firmware_node_t* restrict node, uint64_t* restrict address)
{
    uacpi_u64 value;

    if(uacpi_unlikely_error(uacpi_eval_adr(namespace_node_of(node), &value))) {
        return -ENOENT;
    }

    *address = value;

    return 0;
}

static int acpi_node_get_id(const firmware_node_t* restrict node, char* restrict buffer, size_t size)
{
    uacpi_namespace_node* namespace_node = namespace_node_of(node);
    uacpi_id_string* hid;

    if(uacpi_likely_success(uacpi_eval_hid(namespace_node, &hid))) {
        kstrncpy(buffer, hid->value, size - 1);
        buffer[size - 1] = 0;
        uacpi_free_id_string(hid);
        return 0;
    }

    uacpi_pnp_id_list* cids;

    if(uacpi_likely_success(uacpi_eval_cid(namespace_node, &cids))) {
        int retval = -ENOENT;

        if(cids->num_ids) {
            kstrncpy(buffer, cids->ids[0].value, size - 1);
            buffer[size - 1] = 0;
            retval = 0;
        }

        uacpi_free_pnp_id_list(cids);

        return retval;
    }

    return -ENOENT;
}

// A device that isn't there but functions may still have children that are
static uacpi_iteration_decision visit_child(void* arg, uacpi_namespace_node* namespace_node, uacpi_u32 depth)
{
    child_walk_t* walk = arg;
    uint32_t status = status_of(namespace_node);

    if(0 == (status & (STA_PRESENT | STA_FUNCTIONING))) {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    firmware_node_t child = {};
    child.ops = &acpi_node_ops;
    child.handle = namespace_node;

    walk->callback(&child, walk->arg);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void acpi_node_for_each_child(const firmware_node_t* restrict node, void (*callback)(const firmware_node_t* child, void* arg),
    void* arg)
{
    child_walk_t walk = {};
    walk.callback = callback;
    walk.arg = arg;

    uacpi_namespace_for_each_child(namespace_node_of(node), &visit_child, nullptr, UACPI_OBJECT_DEVICE_BIT, 1, &walk);
}

static unsigned trigger_of(uacpi_u8 triggering)
{
    return triggering == UACPI_TRIGGERING_EDGE ? TRIGGER_EDGE : TRIGGER_LEVEL;
}

// An interrupt active on both edges has no single polarity; such are for GPIOs, which take them apart
static unsigned polarity_of(uacpi_u8 polarity)
{
    return polarity == UACPI_POLARITY_ACTIVE_LOW ? ACTIVE_LOW : ACTIVE_HIGH;
}

static void report_range(const resource_walk_t* restrict walk, unsigned type, uint64_t start, uint64_t length, uint64_t translation,
    unsigned flags)
{
    firmware_resource_t resource = {};
    resource.type = type;
    resource.range.start = start;
    resource.range.length = length;
    resource.range.translation = translation;
    resource.range.flags = flags;

    walk->callback(&resource, walk->arg);
}

static void report_irq(const resource_walk_t* restrict walk, uint32_t number, uacpi_u8 triggering, uacpi_u8 polarity, uacpi_u8 sharing,
    unsigned flags)
{
    firmware_resource_t resource = {};
    resource.type = FIRMWARE_RESOURCE_IRQ;
    resource.irq.number = number;
    resource.irq.trigger = trigger_of(triggering);
    resource.irq.polarity = polarity_of(polarity);
    resource.irq.flags = flags;

    if(sharing == UACPI_SHARED) {
        resource.irq.flags |= FIRMWARE_IRQ_SHARED;
    }

    walk->callback(&resource, walk->arg);
}

static void report_dma(const resource_walk_t* restrict walk, uint8_t channel)
{
    firmware_resource_t resource = {};
    resource.type = FIRMWARE_RESOURCE_DMA;
    resource.dma_channel = channel;

    walk->callback(&resource, walk->arg);
}

static bool is_translated(const uacpi_resource_address_common* restrict common)
{
    if(common->type == UACPI_RANGE_MEMORY) {
        return common->attribute.memory.translation == UACPI_IO_MEM_TRANSLATION;
    }

    if(common->type == UACPI_RANGE_IO) {
        return common->attribute.io.translation == UACPI_IO_MEM_TRANSLATION;
    }

    return false;
}

static void report_address(const resource_walk_t* restrict walk, const uacpi_resource_address_common* restrict common, uint64_t start,
    uint64_t length, uint64_t translation)
{
    unsigned flags = 0;
    unsigned type;

    switch(common->type) {
        case UACPI_RANGE_MEMORY:
            type = FIRMWARE_RESOURCE_MEMORY;
            break;

        case UACPI_RANGE_IO:
            type = FIRMWARE_RESOURCE_PORT;
            break;

        case UACPI_RANGE_BUS:
            type = FIRMWARE_RESOURCE_BUS;
            break;

        default:
            return;
    }

    if(common->direction == UACPI_PRODUCER) {
        flags |= FIRMWARE_RANGE_WINDOW;
    }

    if(is_translated(common)) {
        flags |= FIRMWARE_RANGE_OTHER_TYPE;
    }

    if(length) {
        report_range(walk, type, start, length, translation, flags);
    }
}

static void report_irqs(const resource_walk_t* restrict walk, const uacpi_resource_irq* restrict irq)
{
    for(uacpi_u8 i = 0; i < irq->num_irqs; ++i) {
        report_irq(walk, irq->irqs[i], irq->triggering, irq->polarity, irq->sharing, FIRMWARE_IRQ_ISA);
    }
}

static void report_extended_irqs(const resource_walk_t* restrict walk, const uacpi_resource_extended_irq* restrict irq)
{
    for(uacpi_u8 i = 0; i < irq->num_irqs; ++i) {
        report_irq(walk, irq->irqs[i], irq->triggering, irq->polarity, irq->sharing, 0);
    }
}

static void report_dmas(const resource_walk_t* restrict walk, const uacpi_resource_dma* restrict dma)
{
    for(uacpi_u8 i = 0; i < dma->num_channels; ++i) {
        report_dma(walk, dma->channels[i]);
    }
}

// UNDONE: an extended IRQ with a resource source belongs to an interrupt controller other than the system's
static uacpi_iteration_decision visit_resource(void* arg, uacpi_resource* resource)
{
    const resource_walk_t* walk = arg;

    switch(resource->type) {
        case UACPI_RESOURCE_TYPE_IO:
            report_range(walk, FIRMWARE_RESOURCE_PORT, resource->io.minimum, resource->io.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_FIXED_IO:
            report_range(walk, FIRMWARE_RESOURCE_PORT, resource->fixed_io.address, resource->fixed_io.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_MEMORY24:
            report_range(walk, FIRMWARE_RESOURCE_MEMORY, resource->memory24.minimum, resource->memory24.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_MEMORY32:
            report_range(walk, FIRMWARE_RESOURCE_MEMORY, resource->memory32.minimum, resource->memory32.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_FIXED_MEMORY32:
            report_range(walk, FIRMWARE_RESOURCE_MEMORY, resource->fixed_memory32.address, resource->fixed_memory32.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_ADDRESS16:
            report_address(walk, &resource->address16.common, resource->address16.minimum, resource->address16.address_length,
                resource->address16.translation_offset);
            break;

        case UACPI_RESOURCE_TYPE_ADDRESS32:
            report_address(walk, &resource->address32.common, resource->address32.minimum, resource->address32.address_length,
                resource->address32.translation_offset);
            break;

        case UACPI_RESOURCE_TYPE_ADDRESS64:
            report_address(walk, &resource->address64.common, resource->address64.minimum, resource->address64.address_length,
                resource->address64.translation_offset);
            break;

        case UACPI_RESOURCE_TYPE_ADDRESS64_EXTENDED:
            report_address(walk, &resource->address64_extended.common, resource->address64_extended.minimum,
                resource->address64_extended.address_length, resource->address64_extended.translation_offset);
            break;

        case UACPI_RESOURCE_TYPE_IRQ:
            report_irqs(walk, &resource->irq);
            break;

        case UACPI_RESOURCE_TYPE_EXTENDED_IRQ:
            report_extended_irqs(walk, &resource->extended_irq);
            break;

        case UACPI_RESOURCE_TYPE_DMA:
            report_dmas(walk, &resource->dma);
            break;

        default:
            break;
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static int acpi_node_for_each_resource(const firmware_node_t* restrict node,
    void (*callback)(const firmware_resource_t* resource, void* arg), void* arg)
{
    resource_walk_t walk = {};
    walk.callback = callback;
    walk.arg = arg;

    uacpi_status status = uacpi_for_each_device_resource(namespace_node_of(node), "_CRS", &visit_resource, &walk);

    if(status == UACPI_STATUS_NOT_FOUND) {
        return -ENOENT;
    }

    if(uacpi_unlikely_error(status)) {
        return -EIO;
    }

    return 0;
}

static const firmware_node_ops_t acpi_node_ops = {
    .is_present = &acpi_node_is_present,
    .matches = &acpi_node_matches,
    .get_address = &acpi_node_get_address,
    .get_id = &acpi_node_get_id,
    .for_each_child = &acpi_node_for_each_child,
    .for_each_resource = &acpi_node_for_each_resource,
};

int firmware_root(firmware_node_t* restrict root)
{
    uacpi_namespace_node* system_bus = uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB);

    if(system_bus == nullptr) {
        return -ENODEV;
    }

    root->ops = &acpi_node_ops;
    root->handle = system_bus;

    return 0;
}
