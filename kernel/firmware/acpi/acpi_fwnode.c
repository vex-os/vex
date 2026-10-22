#include <uacpi/acpi.h>
#include <uacpi/namespace.h>
#include <uacpi/resources.h>
#include <uacpi/tables.h>
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>
#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/intctl.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>

// ACPI 6.5, 8.4 Declaring processors: a Processor object stands for
// a device of this id, and older firmware such as QEMU's still has them
#define PROCESSOR_DEVICE_ID "ACPI0007"

// ACPI 6.5, 6.3.7 _STA
#define STA_PRESENT     0x01U
#define STA_ENABLED     0x02U
#define STA_FUNCTIONING 0x08U

typedef struct child_walk_s child_walk_t;
typedef struct cpu_walk_s cpu_walk_t;
typedef struct resource_walk_s resource_walk_t;
typedef struct link_walk_s link_walk_t;

struct child_walk_s {
    void (*callback)(const fwnode_t* child, void* arg);
    void* arg;
};

struct cpu_walk_s {
    uint32_t uid;
    uint64_t cpu_id;
    bool found;
};

struct resource_walk_s {
    void (*callback)(const fwnode_resource_t* resource, void* arg);
    void* arg;
};

struct link_walk_s {
    fwnode_irq_t* irq;
    bool found;
};

static const fwnode_ops_t acpi_node_ops;

static uacpi_namespace_node* namespace_node_of(const fwnode_t* restrict node)
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

static bool acpi_node_is_present(const fwnode_t* restrict node)
{
    uint32_t status = status_of(namespace_node_of(node));
    return (status & (STA_PRESENT | STA_ENABLED)) == (STA_PRESENT | STA_ENABLED);
}

static bool is_processor_object(uacpi_namespace_node* restrict namespace_node)
{
    uacpi_object_type type;

    return uacpi_likely_success(uacpi_namespace_node_type(namespace_node, &type)) && type == UACPI_OBJECT_PROCESSOR;
}

static bool acpi_node_matches(const fwnode_t* restrict node, const char* const* restrict ids)
{
    uacpi_namespace_node* namespace_node = namespace_node_of(node);

    if(!is_processor_object(namespace_node)) {
        return uacpi_device_matches_pnp_id(namespace_node, ids);
    }

    for(const char* const* id = ids; *id; id += 1) {
        if(0 == strcmp(*id, PROCESSOR_DEVICE_ID)) {
            return true;
        }
    }

    return false;
}

// _MAT and MADT entries alike, the local APIC's or the x2APIC's
static bool parse_cpu_entry(const struct acpi_entry_hdr* restrict entry, size_t size, uint32_t* restrict uid, uint64_t* restrict cpu_id)
{
    if(size < sizeof(struct acpi_entry_hdr) || entry->length > size) {
        return false;
    }

    if(entry->type == ACPI_MADT_ENTRY_TYPE_LAPIC && entry->length >= sizeof(struct acpi_madt_lapic)) {
        const struct acpi_madt_lapic* lapic = (const struct acpi_madt_lapic*)(entry);
        *uid = lapic->uid;
        *cpu_id = lapic->id;
        return true;
    }

    if(entry->type == ACPI_MADT_ENTRY_TYPE_LOCAL_X2APIC && entry->length >= sizeof(struct acpi_madt_x2apic)) {
        const struct acpi_madt_x2apic* x2apic = (const struct acpi_madt_x2apic*)(entry);
        *uid = x2apic->uid;
        *cpu_id = x2apic->id;
        return true;
    }

    return false;
}

static int cpu_id_from_mat(uacpi_namespace_node* restrict namespace_node, uint64_t* restrict cpu_id)
{
    uacpi_object* mat;

    if(uacpi_unlikely_error(uacpi_eval_simple_buffer(namespace_node, "_MAT", &mat))) {
        return -ENOENT;
    }

    uacpi_data_view view;
    uint32_t uid;
    bool found = uacpi_likely_success(uacpi_object_get_buffer(mat, &view)) && parse_cpu_entry(view.data, view.length, &uid, cpu_id);

    uacpi_object_unref(mat);

    return found ? 0 : -ENOENT;
}

static int uid_of(uacpi_namespace_node* restrict namespace_node, uint32_t* restrict uid)
{
    if(!is_processor_object(namespace_node)) {
        uacpi_u64 value;

        if(uacpi_unlikely_error(uacpi_eval_simple_integer(namespace_node, "_UID", &value))) {
            return -ENOENT;
        }

        *uid = value;
        return 0;
    }

    uacpi_object* processor;

    if(uacpi_unlikely_error(uacpi_eval_typed(namespace_node, nullptr, nullptr, UACPI_OBJECT_PROCESSOR_BIT, &processor))) {
        return -ENOENT;
    }

    uacpi_processor_info info;
    uacpi_status status = uacpi_object_get_processor_info(processor, &info);

    uacpi_object_unref(processor);

    if(uacpi_unlikely_error(status)) {
        return -ENOENT;
    }

    *uid = info.id;

    return 0;
}

static uacpi_iteration_decision find_cpu_entry(uacpi_handle arg, struct acpi_entry_hdr* entry)
{
    cpu_walk_t* walk = arg;
    uint32_t uid;
    uint64_t cpu_id;

    if(parse_cpu_entry(entry, entry->length, &uid, &cpu_id) && uid == walk->uid) {
        walk->cpu_id = cpu_id;
        walk->found = true;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static int cpu_id_from_madt(uacpi_namespace_node* restrict namespace_node, uint64_t* restrict cpu_id)
{
    cpu_walk_t walk = {};

    if(uid_of(namespace_node, &walk.uid) < 0) {
        return -ENOENT;
    }

    uacpi_table madt;

    if(uacpi_unlikely_error(uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt))) {
        return -ENOENT;
    }

    uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &find_cpu_entry, &walk);
    uacpi_table_unref(&madt);

    if(!walk.found) {
        return -ENOENT;
    }

    *cpu_id = walk.cpu_id;

    return 0;
}

static int acpi_node_get_cpu_id(const fwnode_t* restrict node, uint64_t* restrict cpu_id)
{
    uacpi_namespace_node* namespace_node = namespace_node_of(node);
    const char* const ids[] = {
        PROCESSOR_DEVICE_ID,
        nullptr,
    };

    if(!acpi_node_matches(node, ids)) {
        return -ENOENT;
    }

    if(0 == cpu_id_from_mat(namespace_node, cpu_id)) {
        return 0;
    }

    return cpu_id_from_madt(namespace_node, cpu_id);
}

static int acpi_node_get_address(const fwnode_t* restrict node, uint64_t* restrict address)
{
    uacpi_u64 value;

    if(uacpi_unlikely_error(uacpi_eval_adr(namespace_node_of(node), &value))) {
        return -ENOENT;
    }

    *address = value;

    return 0;
}

static int acpi_node_get_id(const fwnode_t* restrict node, char* restrict buffer, size_t size)
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

// ACPI 6.5, 20.2.2: a name segment is four characters, padded with underscores
static int acpi_node_get_name(const fwnode_t* restrict node, char* restrict buffer, size_t size)
{
    uacpi_object_name name = uacpi_namespace_node_name(namespace_node_of(node));
    char segment[sizeof(name.text) + 1] = {};
    size_t length = sizeof(name.text);

    while(length > 1 && name.text[length - 1] == '_') {
        length -= 1;
    }

    memcpy(segment, name.text, length);

    int formatted = snprintf(buffer, size, "acpi:%s", segment);

    if(formatted < 0) {
        return -EIO;
    }

    if((size_t)(formatted) >= size) {
        return -ERANGE;
    }

    return 0;
}

// A device that isn't there but functions may still have children that are
static uacpi_iteration_decision visit_child(void* arg, uacpi_namespace_node* namespace_node, uacpi_u32 depth)
{
    child_walk_t* walk = arg;
    uint32_t status = status_of(namespace_node);

    if(0 == (status & (STA_PRESENT | STA_FUNCTIONING))) {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    fwnode_t child = {};
    child.ops = &acpi_node_ops;
    child.handle = namespace_node;

    walk->callback(&child, walk->arg);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void acpi_node_for_each_child(const fwnode_t* restrict node, void (*callback)(const fwnode_t* child, void* arg), void* arg)
{
    child_walk_t walk = {};
    walk.callback = callback;
    walk.arg = arg;

    uacpi_namespace_for_each_child(namespace_node_of(node), &visit_child, nullptr, UACPI_OBJECT_DEVICE_BIT | UACPI_OBJECT_PROCESSOR_BIT, 1,
        &walk);
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
    fwnode_resource_t resource = {};
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
    fwnode_resource_t resource = {};
    resource.type = FWNODE_RESOURCE_IRQ;
    resource.irq.number = number;
    resource.irq.trigger = trigger_of(triggering);
    resource.irq.polarity = polarity_of(polarity);
    resource.irq.flags = flags;

    if(sharing == UACPI_SHARED) {
        resource.irq.flags |= FWNODE_IRQ_SHARED;
    }

    walk->callback(&resource, walk->arg);
}

static void report_dma(const resource_walk_t* restrict walk, uint8_t channel)
{
    fwnode_resource_t resource = {};
    resource.type = FWNODE_RESOURCE_DMA;
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
            type = FWNODE_RESOURCE_MEMORY;
            break;

        case UACPI_RANGE_IO:
            type = FWNODE_RESOURCE_PORT;
            break;

        case UACPI_RANGE_BUS:
            type = FWNODE_RESOURCE_BUS;
            break;

        default:
            return;
    }

    if(common->direction == UACPI_PRODUCER) {
        flags |= FWNODE_RANGE_WINDOW;
    }

    if(is_translated(common)) {
        flags |= FWNODE_RANGE_OTHER_TYPE;
    }

    if(length) {
        report_range(walk, type, start, length, translation, flags);
    }
}

static void report_irqs(const resource_walk_t* restrict walk, const uacpi_resource_irq* restrict irq)
{
    for(uacpi_u8 i = 0; i < irq->num_irqs; ++i) {
        report_irq(walk, irq->irqs[i], irq->triggering, irq->polarity, irq->sharing, FWNODE_IRQ_ISA);
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
            report_range(walk, FWNODE_RESOURCE_PORT, resource->io.minimum, resource->io.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_FIXED_IO:
            report_range(walk, FWNODE_RESOURCE_PORT, resource->fixed_io.address, resource->fixed_io.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_MEMORY24:
            report_range(walk, FWNODE_RESOURCE_MEMORY, resource->memory24.minimum, resource->memory24.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_MEMORY32:
            report_range(walk, FWNODE_RESOURCE_MEMORY, resource->memory32.minimum, resource->memory32.length, 0, 0);
            break;

        case UACPI_RESOURCE_TYPE_FIXED_MEMORY32:
            report_range(walk, FWNODE_RESOURCE_MEMORY, resource->fixed_memory32.address, resource->fixed_memory32.length, 0, 0);
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

static int acpi_node_for_each_resource(const fwnode_t* restrict node, void (*callback)(const fwnode_resource_t* resource, void* arg),
    void* arg)
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

static void take_link_irq(link_walk_t* restrict walk, uint32_t number, uacpi_u8 triggering, uacpi_u8 polarity, uacpi_u8 sharing)
{
    walk->irq->number = number;
    walk->irq->trigger = trigger_of(triggering);
    walk->irq->polarity = polarity_of(polarity);
    walk->irq->flags = sharing == UACPI_SHARED ? FWNODE_IRQ_SHARED : 0;
    walk->found = true;
}

// A link's interrupt is a GSI even when a legacy IRQ descriptor gives it
static uacpi_iteration_decision visit_link_resource(void* arg, uacpi_resource* resource)
{
    link_walk_t* walk = arg;

    if(resource->type == UACPI_RESOURCE_TYPE_IRQ && resource->irq.num_irqs) {
        take_link_irq(walk, resource->irq.irqs[0], resource->irq.triggering, resource->irq.polarity, resource->irq.sharing);
        return UACPI_ITERATION_DECISION_BREAK;
    }

    if(resource->type == UACPI_RESOURCE_TYPE_EXTENDED_IRQ && resource->extended_irq.num_irqs) {
        take_link_irq(walk, resource->extended_irq.irqs[0], resource->extended_irq.triggering, resource->extended_irq.polarity,
            resource->extended_irq.sharing);
        return UACPI_ITERATION_DECISION_BREAK;
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

// UNDONE: a link without an interrupt needs one picked from its _PRS and set through _SRS
static int route_through_link(uacpi_namespace_node* restrict link, fwnode_irq_t* restrict irq)
{
    link_walk_t walk = {};
    walk.irq = irq;

    if(uacpi_unlikely_error(uacpi_for_each_device_resource(link, "_CRS", &visit_link_resource, &walk))) {
        return -ENXIO;
    }

    if(!walk.found) {
        return -ENXIO;
    }

    return 0;
}

static const uacpi_pci_routing_table_entry* find_route(const uacpi_pci_routing_table* restrict table, unsigned slot, unsigned pin)
{
    for(uacpi_size i = 0; i < table->num_entries; ++i) {
        const uacpi_pci_routing_table_entry* entry = &table->entries[i];

        if(slot == (entry->address >> 16) && entry->pin == pin) {
            return entry;
        }
    }

    return nullptr;
}

// ACPI 6.5, 6.2.13 _PRT: without a source the index is the GSI, level-triggered and active-low
static int route_entry(const uacpi_pci_routing_table_entry* restrict entry, fwnode_irq_t* restrict irq)
{
    if(entry->source) {
        return route_through_link(entry->source, irq);
    }

    irq->number = entry->index;
    irq->trigger = TRIGGER_LEVEL;
    irq->polarity = ACTIVE_LOW;
    irq->flags = FWNODE_IRQ_SHARED;

    return 0;
}

static int acpi_node_route_pci_interrupt(const fwnode_t* restrict node, unsigned slot, unsigned pin, fwnode_irq_t* restrict irq)
{
    uacpi_pci_routing_table* table;

    if(uacpi_unlikely_error(uacpi_get_pci_routing_table(namespace_node_of(node), &table))) {
        return -ENOENT;
    }

    const uacpi_pci_routing_table_entry* entry = find_route(table, slot, pin);

    int retval;

    if(entry == nullptr) {
        retval = -ENOENT;
    }
    else {
        retval = route_entry(entry, irq);
    }

    uacpi_free_pci_routing_table(table);

    return retval;
}

// ACPI 6.5, 6.5.8 _DEP: a package of references, which uACPI leaves
// as name paths, since the providers may be defined further on
static int acpi_node_for_each_dependency(const fwnode_t* restrict node, void (*callback)(const fwnode_t* provider, void* arg), void* arg)
{
    uacpi_namespace_node* namespace_node = namespace_node_of(node);
    uacpi_object* dependencies;

    if(uacpi_unlikely_error(uacpi_eval_simple_package(namespace_node, "_DEP", &dependencies))) {
        return -ENOENT;
    }

    uacpi_object_array array;

    if(uacpi_unlikely_error(uacpi_object_get_package(dependencies, &array))) {
        uacpi_object_unref(dependencies);
        return -ENOENT;
    }

    for(uacpi_size i = 0; i < array.count; ++i) {
        uacpi_namespace_node* target = nullptr;

        if(!uacpi_object_is_aml_namepath(array.objects[i])) {
            continue;
        }

        if(uacpi_unlikely_error(uacpi_object_resolve_as_aml_namepath(array.objects[i], namespace_node, &target))) {
            continue;
        }

        fwnode_t provider = {};
        provider.ops = &acpi_node_ops;
        provider.handle = target;

        callback(&provider, arg);
    }

    uacpi_object_unref(dependencies);

    return 0;
}

static const fwnode_ops_t acpi_node_ops = {
    .is_present = &acpi_node_is_present,
    .matches = &acpi_node_matches,
    .get_address = &acpi_node_get_address,
    .get_cpu_id = &acpi_node_get_cpu_id,
    .get_id = &acpi_node_get_id,
    .get_name = &acpi_node_get_name,
    .for_each_child = &acpi_node_for_each_child,
    .for_each_resource = &acpi_node_for_each_resource,
    .route_pci_interrupt = &acpi_node_route_pci_interrupt,
    .for_each_dependency = &acpi_node_for_each_dependency,
};

int fwnode_root(fwnode_t* restrict root)
{
    uacpi_namespace_node* system_bus = uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB);

    if(system_bus == nullptr) {
        return -ENODEV;
    }

    root->ops = &acpi_node_ops;
    root->handle = system_bus;

    return 0;
}
