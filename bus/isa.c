// SPDX-License-Identifier: BSD-3-Clause

#include <arch/ireq.h>
#include <bus/isa.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/intctl.h>
#include <core/ireq.h>
#include <core/kprintf.h>
#include <core/panic.h>
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

    if(entry->type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
        iso = (struct acpi_madt_interrupt_source_override*)(entry);

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

static void try_bind_device(struct isa_device* restrict device, struct isa_driver* restrict driver)
{
    if(driver->type == ISA_DRIVER_PROBING) {
        return; // driver is probe-only
    }

    if(device->driver) {
        return; // already bound to a driver
    }

    if(0 == driver->ops->probe(device)) {
        device->driver = driver;
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
        try_bind_device(device, driver);
    }

    return 0;
}

int unregister_isa_device(struct isa_device* restrict device)
{
    struct isa_device* it;
    struct isa_device* prev = nullptr;

    kassert(device && "invalid ISA device pointer");

    if(device_head == device) {
        device_head = device_head->next;
    }
    else {
        for(it = device_head; it; it = it->next) {
            if(it == device) {
                prev->next = it->next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENODEV;
        }
    }

    if(device->driver && device->driver->ops->remove) {
        if(device->driver->ops->remove)
            device->driver->ops->remove(device);
        device->driver = nullptr;
    }

    kprintf(KP_INFORM, "isa: removing device: %s", device->identifier);

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
        try_bind_device(device, driver);
    }

    return 0;
}

int unregister_isa_driver(struct isa_driver* restrict driver)
{
    struct isa_driver* it;
    struct isa_driver* prev = nullptr;
    struct isa_device* device;

    kassert(driver && "invalid ISA driver pointer");

    if(driver_head == driver) {
        driver_head = driver_head->next;
    }
    else {
        for(it = driver_head; it; it = it->next) {
            if(it == driver) {
                prev->next = it->next;
                break;
            }
            prev = it;
        }

        if(!it)
            return ENODEV;
    }

    if(driver->ops->remove) {
        for(device = device_head; device; device = device->next) {
            if(device->driver == driver) {
                if(device->driver->ops->remove)
                    device->driver->ops->remove(device);
                device->driver = nullptr;
            }
        }
    }

    kprintf(KP_INFORM, "isa: removing driver: %s", driver->identifier);

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

    res = isa_get_resource(device, ISA_RESOURCE_IRQ, index);

    if(res == nullptr) {
        return ENODEV;
    }

    isa_irq_route(res->irq.line, &res->irq.gsi, &res->irq.trigger, &res->irq.polarity);

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

    res = isa_get_resource(device, ISA_RESOURCE_IRQ, index);

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

void isa_irq_route(uint8_t line, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity)
{
    if(line < ISA_IRQ_COUNT) {
        out_gsi[0] = isa_irq_table[line].gsi;
        out_trigger[0] = isa_irq_table[line].trigger;
        out_polarity[0] = isa_irq_table[line].polarity;
    }
    else {
        out_gsi[0] = line;
        out_trigger[0] = TRIGGER_EDGE;
        out_polarity[0] = ACTIVE_HIGH;
    }
}

uint8_t isa_read8(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port)
{
    kassert(device && "invalid ISA device pointer");
    kassert(pmio && "invalid PMIO resource pointer");

    return arch_isa_read8(pmio, device->pmio_privdata, port);
}

uint16_t isa_read16(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port)
{
    kassert(device && "invalid ISA device pointer");
    kassert(pmio && "invalid PMIO resource pointer");

    return arch_isa_read16(pmio, device->pmio_privdata, port);
}

void isa_write8(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port, uint8_t value)
{
    kassert(device && "invalid ISA device pointer");
    kassert(pmio && "invalid PMIO resource pointer");

    arch_isa_write8(pmio, device->pmio_privdata, port, value);
}

void isa_write16(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port, uint16_t value)
{
    kassert(device && "invalid ISA device pointer");
    kassert(pmio && "invalid PMIO resource pointer");

    arch_isa_write16(pmio, device->pmio_privdata, port, value);
}

static int init_isa(void)
{
    uacpi_table madt;
    uacpi_status status;
    uint8_t line;

    for(line = 0; line < ISA_IRQ_COUNT; line += 1) {
        isa_irq_table[line].gsi = line;
        isa_irq_table[line].trigger = TRIGGER_EDGE;
        isa_irq_table[line].polarity = ACTIVE_HIGH;
    }

    status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &madt);

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_WARNING, "isa: madt not found, IRQs are identity mapped");
        return 0;
    }

    uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &isa_iso_madt_callback, nullptr);
    uacpi_table_unref(&madt);

    return 0;
}

DEFINE_INITFN_BSP(init_isa, INITFN_STAGE_BSP_BUS, INITFN_ORDER_MIDDLE);
