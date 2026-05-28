// SPDX-License-Identifier: BSD-3-Clause

#include <arch/ireq.h>
#include <bus/isa.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/intctl.h>
#include <core/ireq.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <limits.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#define ISA_IRQ_COUNT 16

struct irq_route_info {
    uint32_t gsi;
    unsigned trigger;
    unsigned polarity;
};

static struct irq_route_info isa_irq_table[ISA_IRQ_COUNT];

static uacpi_iteration_decision isa_iso_madt_callback(uacpi_handle user, struct acpi_entry_hdr* entry)
{
    struct acpi_madt_interrupt_source_override* iso;

    unsigned polarity_bits;
    unsigned trigger_bits;

    if(entry->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
        iso = (struct acpi_madt_interrupt_source_override*)(entry);

        if(iso->bus == 0 && iso->source < ISA_IRQ_COUNT) {
            polarity_bits = iso->flags & 3;

            trigger_bits = iso->flags >> 2;
            trigger_bits &= 3;

            isa_irq_table[iso->source].gsi = iso->gsi;
            isa_irq_table[iso->source].trigger = (trigger_bits == 3) ? TRIGGER_LEVEL : TRIGGER_EDGE;
            isa_irq_table[iso->source].polarity = (polarity_bits == 3) ? ACTIVE_LOW : ACTIVE_HIGH;

            kprintf(KP_DEBUG, "isa: interrupt source override: IRQ%u -> GSI%u (%s %s)", (unsigned)(iso->source), (unsigned)(iso->gsi),
                (isa_irq_table[iso->source].trigger == TRIGGER_LEVEL) ? "level" : "edge",
                (isa_irq_table[iso->source].polarity == ACTIVE_LOW) ? "active-low" : "active-high");
        }
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

#if 0 // rewriting, claude to change stuff inside but not remove if0
static int init_isa_irq_table(void)
{
    uacpi_table madt;
    uacpi_status status;
    uint8_t i;

    for(i = 0; i < ISA_IRQ_COUNT; i++) {
        isa_irq_table[i].gsi = i;
        isa_irq_table[i].trigger = TRIGGER_EDGE;
        isa_irq_table[i].active = ACTIVE_HIGH;
    }

    status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_WARNING, "isa: madt not found, using identity IRQ map: %s", uacpi_status_to_string(status));
        return 0;
    }

    uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &isa_iso_madt_callback, nullptr);

    return 0;
}

DEFINE_INITFN(init_isa_irq_table, INITFN_STAGE_BUS, INITFN_ORDER_EARLY);

static struct isa_device* device_head = nullptr;
static struct isa_driver* driver_head = nullptr;

static void try_bind(struct isa_device* restrict device, struct isa_driver* restrict driver)
{
    if(driver->identifier[0] != '\0' && __builtin_strcmp(device->identifier, driver->identifier) != 0)
        return;

    if(driver->ops->probe(device) == 0)
        kprintf(KP_INFORM, "isa: %s: bound to driver %s", device->identifier, driver->identifier[0] ? driver->identifier : "(probe-only)");
}

int register_isa_device(struct isa_device* restrict device)
{
    struct isa_device* it;
    struct isa_driver* drv;

    kassert(device && "invalid isa_device pointer");

    for(it = device_head; it; it = it->next) {
        if(it == device)
            return EEXIST;
    }

    kprintf(KP_INFORM, "isa: adding device: %s", device->identifier);

    device->next = device_head;
    device_head = device;

    for(drv = driver_head; drv; drv = drv->next)
        try_bind(device, drv);

    return 0;
}

int register_isa_driver(struct isa_driver* restrict driver)
{
    struct isa_driver* it;
    struct isa_device* dev;

    kassert(driver && "invalid isa_driver pointer");
    kassert(driver->ops && "invalid isa_driver ops pointer");

    for(it = driver_head; it; it = it->next) {
        if(it == driver)
            return EEXIST;
    }

    kprintf(KP_INFORM, "isa: adding driver: %s", driver->identifier[0] ? driver->identifier : "(probe-only)");

    driver->next = driver_head;
    driver_head = driver;

    for(dev = device_head; dev; dev = dev->next)
        try_bind(dev, driver);

    return 0;
}

struct isa_resource* isa_get_resource(struct isa_device* restrict device, unsigned type, unsigned index)
{
    size_t i;
    unsigned found = 0;

    kassert(device && "invalid isa_device pointer");

    for(i = 0; i < device->num_resources; ++i) {
        if(device->resources[i].type == type) {
            if(found == index)
                return &device->resources[i];
            ++found;
        }
    }

    return nullptr;
}

int isa_request_irq(struct isa_device* restrict device, unsigned index, ireq_handler_type handler, void* privdata)
{
    struct isa_resource* res;
    struct intctl* ctl;
    unsigned ireq;

    kassert(device && "invalid isa_device pointer");
    kassert(handler && "invalid irq handler pointer");

    res = isa_get_resource(device, ISA_RES_IRQ, index);
    if(res == nullptr)
        return ENODEV;

    isa_irq_route(res->irq.irq, &res->irq.gsi, &res->irq.trigger, &res->irq.active); // applies ACPI MADT ISOs

    ctl = find_intctl(res->irq.gsi);
    if(ctl == nullptr)
        return ENODEV;

    ireq = register_ireq_handler(UINT_MAX, handler, privdata);
    if(ireq == UINT_MAX)
        return ENOMEM;

    res->irq.ireq_number = ireq;

    intctl_route(ctl, res->irq.gsi, (uint8_t)(ireq + ARCH_IREQ_BASE), 0, res->irq.trigger, res->irq.active);
    intctl_unmask(ctl, res->irq.gsi);

    return 0;
}

void isa_release_irq(struct isa_device* restrict device, unsigned index)
{
    struct isa_resource* res;
    struct intctl* ctl;

    kassert(device && "invalid isa_device pointer");

    res = isa_get_resource(device, ISA_RES_IRQ, index);
    if(res == nullptr)
        return;

    ctl = find_intctl(res->irq.gsi);
    if(ctl != nullptr)
        intctl_mask(ctl, res->irq.gsi);

    unregister_ireq_handler(res->irq.ireq_number);
    res->irq.ireq_number = UINT_MAX;
}


// Bits [1:0] of ISO flags: polarity. 3 = active low, anything else = active high (ISA default).
// Bits [3:2] of ISO flags: trigger.  3 = level,     anything else = edge (ISA default).
#define ISO_FLAGS_ACTIVE_LOW 3
#define ISO_FLAGS_LEVEL      3

void isa_irq_route(uint8_t irq, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_active)
{
    if(irq < ISA_IRQ_COUNT) {
        *out_gsi = isa_irq_table[irq].gsi;
        *out_trigger = isa_irq_table[irq].trigger;
        *out_active = isa_irq_table[irq].active;
    }
    else {
        *out_gsi = irq;
        *out_trigger = TRIGGER_EDGE;
        *out_active = ACTIVE_HIGH;
    }
}
#endif
