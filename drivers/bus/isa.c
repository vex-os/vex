#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex.arch/ireq.h>
#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/isa.h>
#include <vex/limits.h>
#include <vex/panic.h>
#include <vex/pmio.h>
#include <vex/string.h>

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

static void try_bind_device(struct isa_device* restrict device, struct isa_driver* restrict driver)
{
    if(device->priv.driver) {
        return;
    }

    if(0 == driver->ops->probe(device)) {
        device->priv.driver = driver;
    }
}

int add_isa_device(struct isa_device* restrict device)
{
    for(struct isa_device* it = device_head; it; it = it->priv.next) {
        if(it == device) {
            return -EEXIST;
        }
    }

    device->priv.next = device_head;
    device_head = device;

    for(struct isa_driver* it = driver_head; it; it = it->priv.next) {
        try_bind_device(device, it);
    }

    return 0;
}

int remove_isa_device(struct isa_device* restrict device)
{
    if(device_head == device) {
        device_head = device_head->priv.next;
    }
    else {
        struct isa_device* it;
        struct isa_device* prev = nullptr;

        for(it = device_head; it; it = it->priv.next) {
            if(it == device) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return -ENODEV;
        }
    }

    if(device->priv.driver && device->priv.driver->ops->remove) {
        if(device->priv.driver->ops->remove)
            device->priv.driver->ops->remove(device);
        device->priv.driver = nullptr;
    }

    return 0;
}

int add_isa_driver(struct isa_driver* restrict driver)
{
    for(struct isa_driver* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(struct isa_device* it = device_head; it; it = it->priv.next) {
        try_bind_device(it, driver);
    }

    return 0;
}

int remove_isa_driver(struct isa_driver* restrict driver)
{
    if(driver_head == driver) {
        driver_head = driver_head->priv.next;
    }
    else {
        struct isa_driver* it;
        struct isa_driver* prev = nullptr;

        for(it = driver_head; it; it = it->priv.next) {
            if(it == driver) {
                prev->priv.next = it->priv.next;
                break;
            }
            prev = it;
        }

        if(it == nullptr) {
            return -ENODEV;
        }
    }

    if(driver->ops->remove) {
        for(struct isa_device* it = device_head; it; it = it->priv.next) {
            if(it->priv.driver == driver) {
                if(it->priv.driver->ops->remove)
                    it->priv.driver->ops->remove(it);
                it->priv.driver = nullptr;
            }
        }
    }

    return 0;
}

struct isa_resource* isa_get_resource(struct isa_device* restrict device, unsigned type, unsigned index)
{
    unsigned found = 0;

    for(size_t i = 0; i < device->priv.num_resources; ++i) {
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
    struct isa_resource* res = isa_get_resource(device, ISA_RESOURCE_IRQ, index);

    if(res == nullptr) {
        return -ENODEV;
    }

    isa_irq_route(res->irq.line, &res->irq.gsi, &res->irq.trigger, &res->irq.polarity);

    struct intctl* controller = find_intctl(res->irq.gsi);

    if(controller == nullptr) {
        return -ENODEV;
    }

    unsigned ireq_number = add_ireq_handler(UINT_MAX, handler, ctxdata);

    if(ireq_number == UINT_MAX) {
        return -ENOMEM;
    }

    res->irq.ireq_number = ireq_number;

    intctl_route(controller, res->irq.gsi, ireq_number + ARCH_IREQ_BASE, 0, res->irq.trigger, res->irq.polarity);
    intctl_unmask(controller, res->irq.gsi);

    return 0;
}

void isa_release_irq(struct isa_device* restrict device, unsigned index)
{
    struct isa_resource* res = isa_get_resource(device, ISA_RESOURCE_IRQ, index);

    if(res == nullptr) {
        return;
    }

    struct intctl* controller = find_intctl(res->irq.gsi);

    if(controller) {
        intctl_mask(controller, res->irq.gsi);
    }

    remove_ireq_handler(res->irq.ireq_number);

    res->irq.ireq_number = UINT_MAX;
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
}

initcall_bsp_bus(init_isa, INIT_MIDDLE);
