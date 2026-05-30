#include <arch/ireq.h>
#include <bus/isa.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/intctl.h>
#include <core/ireq.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initcall.h>
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

    if(device->priv.driver) {
        return; // already bound to a driver
    }

    if(0 == driver->ops->probe(device)) {
        device->priv.driver = driver;
    }
}

int register_isa_device(struct isa_device* restrict device)
{
    struct isa_device* it;
    struct isa_driver* driver;

    for(it = device_head; it; it = it->priv.next) {
        if(it == device)
            return EEXIST;
        continue;
    }

    device->priv.next = device_head;
    device_head = device;

    for(driver = driver_head; driver; driver = driver->priv.next) {
        try_bind_device(device, driver);
    }

    return 0;
}

int unregister_isa_device(struct isa_device* restrict device)
{
    struct isa_device* it;
    struct isa_device* prev = nullptr;

    if(device_head == device) {
        device_head = device_head->priv.next;
    }
    else {
        for(it = device_head; it; it = it->priv.next) {
            if(it == device) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENODEV;
        }
    }

    if(device->priv.driver && device->priv.driver->ops->remove) {
        if(device->priv.driver->ops->remove)
            device->priv.driver->ops->remove(device);
        device->priv.driver = nullptr;
    }

    pr_inform("isa: removing device: %s", device->identifier);

    return 0;
}

int register_isa_driver(struct isa_driver* restrict driver)
{
    struct isa_driver* it;
    struct isa_device* device;

    for(it = driver_head; it; it = it->priv.next) {
        if(it == driver)
            return EEXIST;
        continue;
    }

    if(driver->identifier[0]) {
        pr_inform("isa: adding driver: %s", driver->identifier);
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(device = device_head; device; device = device->priv.next) {
        try_bind_device(device, driver);
    }

    return 0;
}

int unregister_isa_driver(struct isa_driver* restrict driver)
{
    struct isa_driver* it;
    struct isa_driver* prev = nullptr;
    struct isa_device* device;

    if(driver_head == driver) {
        driver_head = driver_head->priv.next;
    }
    else {
        for(it = driver_head; it; it = it->priv.next) {
            if(it == driver) {
                prev->priv.next = it->priv.next;
                break;
            }
            prev = it;
        }

        if(!it)
            return ENODEV;
    }

    if(driver->ops->remove) {
        for(device = device_head; device; device = device->priv.next) {
            if(device->priv.driver == driver) {
                if(device->priv.driver->ops->remove)
                    device->priv.driver->ops->remove(device);
                device->priv.driver = nullptr;
            }
        }
    }

    pr_inform("isa: removing driver: %s", driver->identifier);

    return 0;
}

struct isa_resource* isa_get_resource(struct isa_device* restrict device, unsigned type, unsigned index)
{
    size_t i;
    unsigned found = 0;

    for(i = 0; i < device->priv.num_resources; ++i) {
        if(device->priv.resources[i].type == type) {
            if(found == index)
                return &device->priv.resources[i];
            found += 1;
        }
    }

    return nullptr;
}

int isa_request_irq(struct isa_device* restrict device, unsigned index, ireq_handler_type handler, void* ctxdata)
{
    struct isa_resource* res;
    struct intctl* controller;
    unsigned ireq_number;

    res = isa_get_resource(device, ISA_RESOURCE_IRQ, index);

    if(res == nullptr) {
        return ENODEV;
    }

    isa_irq_route(res->irq.line, &res->irq.gsi, &res->irq.trigger, &res->irq.polarity);

    controller = find_intctl(res->irq.gsi);

    if(controller == nullptr) {
        return ENODEV;
    }

    ireq_number = register_ireq_handler(UINT_MAX, handler, ctxdata);

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
    return arch_isa_read8(pmio, device->priv.pmio_ctxdata, port);
}

uint16_t isa_read16(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port)
{
    return arch_isa_read16(pmio, device->priv.pmio_ctxdata, port);
}

void isa_write8(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port, uint8_t value)
{
    arch_isa_write8(pmio, device->priv.pmio_ctxdata, port, value);
}

void isa_write16(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port, uint16_t value)
{
    arch_isa_write16(pmio, device->priv.pmio_ctxdata, port, value);
}

define_initcall(isa, INITCALL_STAGE_BSP_BUS, INITCALL_ORDER_MIDDLE)
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

    if(uacpi_likely_success(status)) {
        uacpi_for_each_subtable(madt.hdr, sizeof(struct acpi_madt), &isa_iso_madt_callback, nullptr);
        uacpi_table_unref(&madt);
    }
}
