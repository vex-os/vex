// SPDX-License-Identifier: BSD-3-Clause

#include <arch/ireq.h>
#include <bus/isa.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/intctl.h>
#include <core/ireq.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <lib/string.h>
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
static struct isa_device* device_head = nullptr;
static struct isa_driver* driver_head = nullptr;

static uacpi_iteration_decision isa_iso_madt_callback(uacpi_handle user, struct acpi_entry_hdr* entry)
{
    struct acpi_madt_interrupt_source_override* iso;
    const char* polarity_string;
    const char* trigger_string;

    if(entry->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
        iso = (struct acpi_madt_interrupt_source_override*)(entry);

        if(iso->bus == 0 && iso->source < ISA_IRQ_COUNT) {
            if(3 == (iso->flags & 3)) {
                isa_irq_table[iso->source].polarity = ACTIVE_LOW;
                polarity_string = "active-low";
            }
            else {
                isa_irq_table[iso->source].polarity = ACTIVE_HIGH;
                polarity_string = "active-high";
            }

            if(3 == ((iso->flags >> 2) & 3)) {
                isa_irq_table[iso->source].trigger = TRIGGER_LEVEL;
                trigger_string = "level";
            }
            else {
                isa_irq_table[iso->source].trigger = TRIGGER_EDGE;
                trigger_string = "edge";
            }

            kprintf(KP_DEBUG, "isa: interrupt source override: IRQ%u -> GSI%u (%s %s)", (unsigned)(iso->source), (unsigned)(iso->gsi),
                trigger_string, polarity_string);

            isa_irq_table[iso->source].gsi = iso->gsi;
        }
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void try_bind(struct isa_device* restrict device, struct isa_driver* restrict driver)
{
    if(driver->identifier[0] && strcmp(device->identifier, driver->identifier)) {
        return; // it's a probe-only device
    }

    if(0 == driver->ops->probe(device)) {
        if(driver->identifier[0]) {
            kprintf(KP_INFORM, "isa: %s: bound to driver %s", device->identifier, driver->identifier);
        }
        else {
            kprintf(KP_INFORM, "isa: %s: bound to probe-only driver", device->identifier);
        }
    }
}

int register_isa_device(struct isa_device* restrict device)
{
    struct isa_device* it;
    struct isa_driver* driver;

    kassert(device && "invalid ISA device pointer");

    for(it = device_head; it; it = it->next) {
        if(it == device)
            return EEXIST;
        continue;
    }

    kprintf(KP_INFORM, "isa: adding device: %s", device->identifier);

    device->next = device_head;
    device_head = device;

    for(driver = driver_head; driver; driver = driver->next) {
        try_bind(device, driver);
    }

    return 0;
}

int register_isa_driver(struct isa_driver* restrict driver)
{
    struct isa_driver* it;
    struct isa_device* device;

    kassert(driver && "invalid ISA driver pointer");
    kassert(driver->ops && "invalid ISA driver ops pointer");

    for(it = driver_head; it; it = it->next) {
        if(it == driver)
            return EEXIST;
        continue;
    }

    if(driver->identifier[0]) {
        kprintf(KP_INFORM, "isa: adding driver: %s", driver->identifier);
    }

    driver->next = driver_head;
    driver_head = driver;

    for(device = device_head; device; device = device->next) {
        try_bind(device, driver);
    }

    return 0;
}

struct isa_resource* isa_get_resource(struct isa_device* restrict device, unsigned type, unsigned index)
{
    size_t i;
    unsigned found = 0;

    kassert(device && "invalid ISA device pointer");

    for(i = 0; i < device->num_resources; ++i) {
        if(device->resources[i].type == type) {
            if(found == index)
                return &device->resources[i];
            found += 1;
        }
    }

    return nullptr;
}

int isa_request_irq(struct isa_device* restrict device, unsigned index, ireq_handler_type handler, void* privdata)
{
    struct isa_resource* res;
    struct intctl* controller;
    unsigned ireq_number;

    kassert(device && "invalid ISA device pointer");
    kassert(handler && "invalid IRQ handler pointer");

    res = isa_get_resource(device, ISA_RES_IRQ, index);

    if(res == nullptr) {
        return ENODEV;
    }

    isa_irq_route(res->irq.irq, &res->irq.gsi, &res->irq.trigger, &res->irq.polarity);

    controller = find_intctl(res->irq.gsi);

    if(controller == nullptr) {
        return ENODEV;
    }

    ireq_number = register_ireq_handler(UINT_MAX, handler, privdata);

    if(ireq_number == UINT_MAX) {
        return ENOMEM;
    }

    res->irq.ireq_number = ireq_number;

    intctl_route(controller, res->irq.gsi, (uint8_t)(ireq_number + ARCH_IREQ_BASE), 0, res->irq.trigger, res->irq.polarity);
    intctl_unmask(controller, res->irq.gsi);

    return 0;
}

void isa_release_irq(struct isa_device* restrict device, unsigned index)
{
    struct isa_resource* res;
    struct intctl* controller;

    kassert(device && "invalid ISA device pointer");

    res = isa_get_resource(device, ISA_RES_IRQ, index);

    if(res == nullptr) {
        return;
    }

    controller = find_intctl(res->irq.gsi);

    if(controller) {
        intctl_mask(controller, res->irq.gsi);
    }

    unregister_ireq_handler(res->irq.ireq_number);

    res->irq.ireq_number = UINT_MAX;
}

void isa_irq_route(uint8_t irq, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity)
{
    if(irq < ISA_IRQ_COUNT) {
        out_gsi[0] = isa_irq_table[irq].gsi;
        out_trigger[0] = isa_irq_table[irq].trigger;
        out_polarity[0] = isa_irq_table[irq].polarity;
    }
    else {
        out_gsi[0] = irq;
        out_trigger[0] = TRIGGER_EDGE;
        out_polarity[0] = ACTIVE_HIGH;
    }
}

static int init_isa_irq_routine(void)
{
    uacpi_table madt;
    uacpi_status status;
    uint8_t i;

    for(i = 0; i < ISA_IRQ_COUNT; ++i) {
        isa_irq_table[i].gsi = i;
        isa_irq_table[i].trigger = TRIGGER_EDGE;
        isa_irq_table[i].polarity = ACTIVE_HIGH;
    }

    status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_WARNING, "isa: madt not found, IRQs are identity mapped");
        return 0;
    }

    uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &isa_iso_madt_callback, nullptr);
    uacpi_table_unref(&madt); // should we do it here?

    return 0;
}

DEFINE_INITFN(init_isa_irq_routine, INITFN_STAGE_BUS, INITFN_ORDER_EARLY);
