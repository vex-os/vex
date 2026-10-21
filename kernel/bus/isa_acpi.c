#include <uacpi/namespace.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/isa.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/string.h>

#define ACPI_MAX_ISA_RESOURCES 16

typedef struct parse_context_s parse_context_t;

struct parse_context_s {
    isa_resource_t resources[ACPI_MAX_ISA_RESOURCES];
    unsigned num_resources;
};

static uacpi_iteration_decision parse_resource(void* ctxdata, uacpi_resource* acpi_resource)
{
    parse_context_t* context = ctxdata;

    if(context->num_resources < ACPI_MAX_ISA_RESOURCES) {
        isa_resource_t* resource = &context->resources[context->num_resources];

        if(acpi_resource->type == UACPI_RESOURCE_TYPE_IO) {
            resource->type = ISA_RESOURCE_PMIO;
            resource->io.space = IO_SPACE_PORT;
            resource->io.base = acpi_resource->io.minimum;
            resource->io.length = acpi_resource->io.length;
            context->num_resources += 1;
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        if(acpi_resource->type == UACPI_RESOURCE_TYPE_FIXED_IO) {
            resource->type = ISA_RESOURCE_PMIO;
            resource->io.space = IO_SPACE_PORT;
            resource->io.base = acpi_resource->fixed_io.address;
            resource->io.length = acpi_resource->fixed_io.length;
            context->num_resources += 1;
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        if(acpi_resource->type == UACPI_RESOURCE_TYPE_IRQ && acpi_resource->irq.num_irqs) {
            resource->type = ISA_RESOURCE_IRQ;
            resource->irq.line = acpi_resource->irq.irqs[0];
            resource->irq.gsi = 0;
            resource->irq.irq_number = UINT_MAX;

            if(acpi_resource->irq.triggering == UACPI_TRIGGERING_EDGE) {
                resource->irq.trigger = TRIGGER_EDGE;
            }
            else {
                resource->irq.trigger = TRIGGER_LEVEL;
            }

            if(acpi_resource->irq.polarity == UACPI_POLARITY_ACTIVE_HIGH) {
                resource->irq.polarity = ACTIVE_HIGH;
            }
            else {
                resource->irq.polarity = ACTIVE_LOW;
            }

            context->num_resources += 1;
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        if(acpi_resource->type == UACPI_RESOURCE_TYPE_EXTENDED_IRQ && acpi_resource->extended_irq.num_irqs) {
            resource->type = ISA_RESOURCE_IRQ;
            resource->irq.line = acpi_resource->extended_irq.irqs[0];
            resource->irq.gsi = 0;
            resource->irq.irq_number = UINT_MAX;

            if(acpi_resource->extended_irq.triggering == UACPI_TRIGGERING_EDGE) {
                resource->irq.trigger = TRIGGER_EDGE;
            }
            else {
                resource->irq.trigger = TRIGGER_LEVEL;
            }

            if(acpi_resource->extended_irq.polarity == UACPI_POLARITY_ACTIVE_HIGH) {
                resource->irq.polarity = ACTIVE_HIGH;
            }
            else {
                resource->irq.polarity = ACTIVE_LOW;
            }

            context->num_resources += 1;
            return UACPI_ITERATION_DECISION_CONTINUE;
        }
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static uacpi_iteration_decision enumerate_device(void* ctxdata, uacpi_namespace_node* node, uacpi_u32 depth)
{
    uacpi_namespace_node_info* info;
    uacpi_status status = uacpi_get_namespace_node_info(node, &info);

    if(uacpi_unlikely_error(status)) {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    if(0 == (info->flags & UACPI_NS_NODE_INFO_HAS_HID)) {
        uacpi_free_namespace_node_info(info);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    isa_device_t* device = kmalloc(sizeof(isa_device_t));

    if(device == nullptr) {
        uacpi_free_namespace_node_info(info);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    device->name = kstrdup(info->hid.value);

    if(device->name == nullptr) {
        pr_warning("acpi_isa: out of memory");

        uacpi_free_namespace_node_info(info);

        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    parse_context_t context;
    context.num_resources = 0;

    uacpi_for_each_device_resource(node, "_CRS", parse_resource, &context);

    isa_resource_t* resources = nullptr;

    if(context.num_resources > 0) {
        resources = kmalloc(sizeof(isa_resource_t) * context.num_resources);

        if(resources == nullptr) {
            context.num_resources = 0;
        }
        else {
            memcpy(resources, context.resources, sizeof(isa_resource_t) * context.num_resources);
        }
    }

    device->priv.next = nullptr;
    device->priv.driver = nullptr;
    device->priv.resources = resources;
    device->priv.num_resources = context.num_resources;
    device->priv.acpi_node = node;
    device->ctxdata = nullptr;

    pr_verbose("acpi_isa: found %s: num_resources=%u", device->name, context.num_resources);

    isa_device_add(device);

    uacpi_free_namespace_node_info(info);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static void init_isa_acpi(void)
{
    uacpi_namespace_node* node = uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB);
    uacpi_status status = uacpi_namespace_for_each_child(node, &enumerate_device, nullptr, UACPI_OBJECT_DEVICE_BIT, UACPI_MAX_DEPTH_ANY,
        nullptr);

    if(uacpi_unlikely_error(status)) {
        pr_verbose("isa_acpi: %s", uacpi_status_to_string(status));
    }
}

initcall_bsp_bus(init_isa_acpi, INIT_LATE);
