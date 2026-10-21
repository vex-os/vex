#include <vex/fwnode.h>
#include <vex/initcall.h>
#include <vex/isa.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/string.h>

#define ACPI_MAX_ISA_RESOURCES 16
#define ACPI_ID_MAX            16

typedef struct parse_context_s parse_context_t;

struct parse_context_s {
    isa_resource_t resources[ACPI_MAX_ISA_RESOURCES];
    unsigned num_resources;
};

static void parse_resource(const fwnode_resource_t* resource, void* arg)
{
    parse_context_t* context = arg;

    // A window is what a bridge passes on, not a register range of the device
    if(context->num_resources == ACPI_MAX_ISA_RESOURCES
        || (resource->type == FWNODE_RESOURCE_PORT && (resource->range.flags & FWNODE_RANGE_WINDOW))) {
        return;
    }

    isa_resource_t* parsed = &context->resources[context->num_resources];

    switch(resource->type) {
        case FWNODE_RESOURCE_PORT:
            parsed->type = ISA_RESOURCE_PMIO;
            parsed->io.space = IO_SPACE_PORT;
            parsed->io.base = resource->range.start;
            parsed->io.length = resource->range.length;
            break;

        case FWNODE_RESOURCE_IRQ:
            parsed->type = ISA_RESOURCE_IRQ;
            parsed->irq.line = resource->irq.number;
            parsed->irq.gsi = 0;
            parsed->irq.trigger = resource->irq.trigger;
            parsed->irq.polarity = resource->irq.polarity;
            parsed->irq.irq_number = UINT_MAX;
            break;

        default:
            return;
    }

    context->num_resources += 1;
}

static void add_device(const fwnode_t* restrict node, const char* restrict id)
{
    isa_device_t* device = kmalloc(sizeof(isa_device_t));

    if(device == nullptr) {
        pr_warning("acpi_isa: out of memory");
        return;
    }

    device->name = kstrdup(id);

    if(device->name == nullptr) {
        pr_warning("acpi_isa: out of memory");
        kfree(device);
        return;
    }

    parse_context_t context;
    context.num_resources = 0;

    fwnode_for_each_resource(node, &parse_resource, &context);

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
    device->priv.node = *node;
    device->ctxdata = nullptr;

    pr_verbose("acpi_isa: found %s: num_resources=%u", device->name, context.num_resources);

    isa_device_add(device);
}

static void visit_node(const fwnode_t* node, void* arg)
{
    char id[ACPI_ID_MAX];

    if(fwnode_is_present(node) && 0 == fwnode_get_id(node, id, sizeof(id))) {
        add_device(node, id);
    }

    fwnode_for_each_child(node, &visit_node, nullptr);
}

static void init_isa_acpi(void)
{
    fwnode_t root;

    if(fwnode_root(&root) < 0) {
        return;
    }

    fwnode_for_each_child(&root, &visit_node, nullptr);
}

initcall_bsp_bus(init_isa_acpi, INIT_LATE);
