// SPDX-License-Identifier: BSD-3-Clause

#include <bus/isa.h>
#include <core/errno.h>
#include <core/intctl.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <lib/string.h>
#include <limits.h>
#include <mm/malloc.h>
#include <mm/slab.h>
#include <uacpi/namespace.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>

#define ACPI_MAX_ISA_RESOURCES 16

struct parse_context {
    struct isa_resource resources[ACPI_MAX_ISA_RESOURCES];
    unsigned num_resources;
};

static uacpi_iteration_decision parse_resource(void* privdata, uacpi_resource* acpi_resource)
{
    struct parse_context* context = privdata;
    struct isa_resource* resource;

    if(context->num_resources < ACPI_MAX_ISA_RESOURCES) {
        resource = &context->resources[context->num_resources];

        if(acpi_resource->type == UACPI_RESOURCE_TYPE_IO) {
            resource->type = ISA_RESOURCE_PMIO;
            resource->pmio.pmio = acpi_resource->io.minimum;
            resource->pmio.length = acpi_resource->io.length;
            resource->pmio.virt8 = nullptr;
            resource->pmio.virt16 = nullptr;
            context->num_resources += 1;
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        if(acpi_resource->type == UACPI_RESOURCE_TYPE_FIXED_IO) {
            resource->type = ISA_RESOURCE_PMIO;
            resource->pmio.pmio = acpi_resource->fixed_io.address;
            resource->pmio.length = acpi_resource->fixed_io.length;
            resource->pmio.virt8 = nullptr;
            resource->pmio.virt16 = nullptr;
            context->num_resources += 1;
            return UACPI_ITERATION_DECISION_CONTINUE;
        }

        if(acpi_resource->type == UACPI_RESOURCE_TYPE_IRQ && acpi_resource->irq.num_irqs) {
            resource->type = ISA_RESOURCE_IRQ;
            resource->irq.line = acpi_resource->irq.irqs[0];
            resource->irq.gsi = 0;
            resource->irq.ireq_number = UINT_MAX;

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
            resource->irq.ireq_number = UINT_MAX;

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

static uacpi_iteration_decision enumerate_device(void* privdata, uacpi_namespace_node* node, uacpi_u32 depth)
{
    uacpi_status status;
    uacpi_namespace_node_info* info;
    struct parse_context context;
    struct isa_device* device;
    struct isa_resource* resources;

    status = uacpi_get_namespace_node_info(node, &info);

    if(uacpi_unlikely_error(status)) {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    if(0 == (info->flags & UACPI_NS_NODE_INFO_HAS_HID)) {
        uacpi_free_namespace_node_info(info);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    device = slab_alloc(sizeof(struct isa_device));

    if(device == nullptr) {
        uacpi_free_namespace_node_info(info);
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    memset(device->identifier, 0, sizeof(device->identifier));
    strncpy(device->identifier, info->hid.value, sizeof(device->identifier) - 1);

    context.num_resources = 0;
    uacpi_for_each_device_resource(node, "_CRS", parse_resource, &context);

    resources = nullptr;

    if(context.num_resources > 0) {
        resources = kmalloc(sizeof(struct isa_resource) * context.num_resources);

        if(resources == nullptr) {
            context.num_resources = 0;
        }
        else {
            memcpy(resources, context.resources, sizeof(struct isa_resource) * context.num_resources);
        }
    }

    device->next = nullptr;
    device->driver = nullptr;
    device->resources = resources;
    device->num_resources = context.num_resources;
    device->pmio_privdata = nullptr;
    device->acpi_node = node;
    device->privdata = nullptr;

    register_isa_device(device);

    uacpi_free_namespace_node_info(info);

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static int init_acpi_isa(void)
{
    uacpi_status status;

    status = uacpi_namespace_for_each_child(uacpi_namespace_get_predefined(UACPI_PREDEFINED_NAMESPACE_SB), enumerate_device, nullptr,
        UACPI_OBJECT_DEVICE_BIT, UACPI_MAX_DEPTH_ANY, nullptr);

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_WARNING, "acpi_isa: %s", uacpi_status_to_string(status));
        return ENODEV;
    }

    return 0;
}

DEFINE_INITFN_BSP(init_acpi_isa, INITFN_STAGE_BSP_BUS, INITFN_ORDER_LATE);
