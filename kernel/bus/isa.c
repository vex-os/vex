#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/isa.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/pci.h>
#include <vex/string.h>

#define ISA_IRQ_COUNT    16
#define ISA_RESOURCE_MAX 16
#define ISA_ID_MAX       16

typedef struct irq_route_info_s irq_route_info_t;
typedef struct resource_list_s resource_list_t;

struct irq_route_info_s {
    uint32_t gsi;
    unsigned trigger;
    unsigned polarity;
};

struct isa_bus_s {
    pci_device_t* bridge;
};

struct resource_list_s {
    fwnode_resource_t resources[ISA_RESOURCE_MAX];
    size_t num_resources;
};

static const pci_driver_id_t bridge_ids[] = {
    PCI_DEVICE_CLASS(0x06, 0x01),
    PCI_DEVICE_END(),
};

static irq_route_info_t isa_irq_table[ISA_IRQ_COUNT];
static mutex_t isa_mutex = MUTEX_INIT;
static isa_device_t* device_head = nullptr;
static isa_device_t** device_tail = &device_head;
static isa_driver_t* driver_head = nullptr;

static uacpi_iteration_decision isa_iso_madt_callback(uacpi_handle user, struct acpi_entry_hdr* entry)
{
    if(entry->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
        struct acpi_madt_interrupt_source_override* iso = (struct acpi_madt_interrupt_source_override*)(entry);

        if(iso->bus == 0 && iso->source < ISA_IRQ_COUNT) {
            if(3 == (iso->flags & 3)) {
                isa_irq_table[iso->source].polarity = ACTIVE_LOW;
            }
            else {
                isa_irq_table[iso->source].polarity = ACTIVE_HIGH;
            }

            if(3 == ((iso->flags >> 2) & 3)) {
                isa_irq_table[iso->source].trigger = TRIGGER_LEVEL;
            }
            else {
                isa_irq_table[iso->source].trigger = TRIGGER_EDGE;
            }

            isa_irq_table[iso->source].gsi = iso->gsi;
        }
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void device_bind(isa_device_t* restrict device, isa_driver_t* restrict driver)
{
    if(device->priv.driver || !fwnode_matches(&device->priv.node, driver->ids)) {
        return;
    }

    if(0 == driver->ops->probe(device)) {
        device->priv.driver = driver;
    }
}

static void collect_resource(const fwnode_resource_t* resource, void* arg)
{
    resource_list_t* list = arg;

    if(list->num_resources == ISA_RESOURCE_MAX) {
        return;
    }

    list->resources[list->num_resources] = *resource;
    list->num_resources += 1;
}

static int copy_resources(isa_device_t* restrict device, const resource_list_t* restrict list)
{
    device->priv.resources = nullptr;
    device->priv.interrupts = nullptr;
    device->priv.num_resources = list->num_resources;

    if(list->num_resources == 0) {
        return 0;
    }

    device->priv.resources = kmalloc(sizeof(fwnode_resource_t) * list->num_resources);
    device->priv.interrupts = kmalloc(sizeof(interrupt_action_t*) * list->num_resources);

    if(device->priv.resources == nullptr || device->priv.interrupts == nullptr) {
        kfree(device->priv.resources);
        kfree(device->priv.interrupts);
        return -ENOMEM;
    }

    memcpy(device->priv.resources, list->resources, sizeof(fwnode_resource_t) * list->num_resources);
    memset(device->priv.interrupts, 0, sizeof(interrupt_action_t*) * list->num_resources);

    return 0;
}

static void add_device(isa_bus_t* restrict bus, const fwnode_t* restrict node, const char* restrict id)
{
    isa_device_t* device = kmalloc(sizeof(isa_device_t));

    if(device == nullptr) {
        pr_warning("isa: out of memory");
        return;
    }

    resource_list_t list = {};
    fwnode_for_each_resource(node, &collect_resource, &list);

    char* name = kstrdup(id);

    if(name == nullptr) {
        pr_warning("isa: out of memory");
        kfree(device);
        return;
    }

    if(copy_resources(device, &list) < 0) {
        pr_warning("isa: out of memory");
        kfree(name);
        kfree(device);
        return;
    }

    device->name = name;
    device->priv.next = nullptr;
    device->priv.bus = bus;
    device->priv.driver = nullptr;
    device->priv.node = *node;
    device->ctxdata = nullptr;

    *device_tail = device;
    device_tail = &device->priv.next;

    pr_verbose("isa: found %s: num_resources=%zu", device->name, device->priv.num_resources);

    for(isa_driver_t* it = driver_head; it; it = it->priv.next) {
        device_bind(device, it);
    }
}

static void visit_child(const fwnode_t* child, void* arg)
{
    isa_bus_t* bus = arg;
    char id[ISA_ID_MAX];

    if(!fwnode_is_present(child)) {
        return;
    }

    if(fwnode_get_id(child, id, sizeof(id)) < 0) {
        return;
    }

    if(fwnode_claim(child) < 0) {
        return;
    }

    add_device(bus, child, id);
}

// UNDONE: a bridge the firmware doesn't describe, such as one on a card, needs its devices found through ISA PnP
static int bridge_probe(pci_device_t* restrict bridge, const pci_driver_id_t* restrict id)
{
    fwnode_t node;

    if(pci_get_companion(bridge, &node) < 0) {
        return -ENODEV;
    }

    isa_bus_t* bus = kmalloc(sizeof(isa_bus_t));

    if(bus == nullptr) {
        return -ENOMEM;
    }

    bus->bridge = bridge;
    bridge->ctxdata = bus;

    mutex_acquire(&isa_mutex);
    fwnode_for_each_child(&node, &visit_child, bus);
    mutex_release(&isa_mutex);

    return 0;
}

static const pci_driver_ops_t bridge_ops = {
    .probe = &bridge_probe,
};

static pci_driver_t bridge_driver = {
    .ops = &bridge_ops,
    .ids = bridge_ids,
};

static fwnode_resource_t* find_resource(isa_device_t* restrict device, bool want_irq, unsigned index, size_t* restrict slot)
{
    unsigned found = 0;

    for(size_t i = 0; i < device->priv.num_resources; ++i) {
        fwnode_resource_t* resource = &device->priv.resources[i];
        bool is_irq = resource->type == FWNODE_RESOURCE_IRQ;
        bool is_registers = resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;
        bool wanted = want_irq ? is_irq : is_registers;

        if(!wanted) {
            continue;
        }

        if(found == index) {
            *slot = i;
            return resource;
        }

        found += 1;
    }

    return nullptr;
}

int isa_driver_add(isa_driver_t* restrict driver)
{
    mutex_acquire(&isa_mutex);

    for(isa_driver_t* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            mutex_release(&isa_mutex);
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(isa_device_t* it = device_head; it; it = it->priv.next) {
        device_bind(it, driver);
    }

    mutex_release(&isa_mutex);

    return 0;
}

int isa_driver_remove(isa_driver_t* restrict driver)
{
    mutex_acquire(&isa_mutex);

    isa_driver_t** prev = &driver_head;

    while(*prev && *prev != driver) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&isa_mutex);
        return -ENODEV;
    }

    *prev = driver->priv.next;

    for(isa_device_t* it = device_head; it; it = it->priv.next) {
        if(it->priv.driver == driver) {
            if(driver->ops->remove) {
                driver->ops->remove(it);
            }

            it->priv.driver = nullptr;
        }
    }

    mutex_release(&isa_mutex);

    return 0;
}

void isa_get_node(const isa_device_t* restrict device, fwnode_t* restrict node)
{
    *node = device->priv.node;
}

int isa_map_io(isa_device_t* restrict device, unsigned index, io_region_t* restrict region)
{
    size_t slot;
    const fwnode_resource_t* resource = find_resource(device, false, index, &slot);

    if(resource == nullptr) {
        return -ENODEV;
    }

    return pci_map_range(device->priv.bus->bridge, resource->type, resource->range.start, resource->range.length, region);
}

void isa_unmap_io(io_region_t* restrict region)
{
    pci_unmap_range(region);
}

int isa_request_irq(isa_device_t* restrict device, unsigned index, irq_handler_t handler, void* restrict ctxdata)
{
    size_t slot;
    const fwnode_resource_t* resource = find_resource(device, true, index, &slot);

    if(resource == nullptr) {
        return -ENODEV;
    }

    if(device->priv.interrupts[slot]) {
        return -EBUSY;
    }

    uint32_t gsi = resource->irq.number;
    unsigned trigger = resource->irq.trigger;
    unsigned polarity = resource->irq.polarity;
    unsigned flags = 0;

    if(resource->irq.flags & FWNODE_IRQ_ISA) {
        isa_irq_route(resource->irq.number, &gsi, &trigger, &polarity);
    }

    if(resource->irq.flags & FWNODE_IRQ_SHARED) {
        flags |= INTERRUPT_SHARED;
    }

    return interrupt_attach(gsi, trigger, polarity, flags, handler, ctxdata, &device->priv.interrupts[slot]);
}

void isa_release_irq(isa_device_t* restrict device, unsigned index)
{
    size_t slot;

    if(find_resource(device, true, index, &slot) == nullptr) {
        return;
    }

    if(device->priv.interrupts[slot] == nullptr) {
        return;
    }

    interrupt_detach(device->priv.interrupts[slot]);

    device->priv.interrupts[slot] = nullptr;
}

void isa_irq_route(uint8_t line, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity)
{
    if(line < ISA_IRQ_COUNT) {
        *out_gsi = isa_irq_table[line].gsi;
        *out_trigger = isa_irq_table[line].trigger;
        *out_polarity = isa_irq_table[line].polarity;
    }
    else {
        *out_gsi = line;
        *out_trigger = TRIGGER_EDGE;
        *out_polarity = ACTIVE_HIGH;
    }
}

static void init_isa(void)
{
    for(uint32_t i = 0; i < ISA_IRQ_COUNT; ++i) {
        isa_irq_table[i].gsi = i;
        isa_irq_table[i].trigger = TRIGGER_EDGE;
        isa_irq_table[i].polarity = ACTIVE_HIGH;
    }

    uacpi_table madt;
    uacpi_status status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);

    if(uacpi_likely_success(status)) {
        uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &isa_iso_madt_callback, nullptr);
        uacpi_table_unref(&madt);
    }

    pci_driver_add(&bridge_driver);
}

initcall_bsp_bus(init_isa, INIT_MIDDLE);
