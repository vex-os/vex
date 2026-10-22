#include <vex/device.h>
#include <vex/device_bus.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/intctl.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/pci.h>
#include <vex/sprintf.h>
#include <vex/string.h>

#define CHANNEL_ID "PNP0600" // ACPI's id of an IDE channel

#define CHANNEL_NAME_MAX      8
#define CHANNEL_RESOURCES_MAX 3 // The command block, the control block and the interrupt

#define NATIVE_CONTROL_OFFSET 2 // PCI IDE Controller 1.0, 2.1: the Device Control port is at offset 2 of a native control block BAR

typedef struct ide_channel_s ide_channel_t;
typedef struct ide_controller_s ide_controller_t;

struct ide_channel_s {
    fwnode_fixed_t fixed; // The channel's firmware node, which outlives the channel's node
    fwnode_resource_t resources[CHANNEL_RESOURCES_MAX];
    char name[CHANNEL_NAME_MAX];
    bool native;
};

struct ide_controller_s {
    ide_channel_t channels[PCI_IDE_CHANNELS];
    unsigned native_irqs; // Native channels with a handler, which share the pin of the function
};

// PCI IDE Controller 1.0, 2.1: each channel in compatibility mode has
// a command block, a control block and an IRQ at the ISA addresses
static const uint64_t compat_command_starts[] = {
    0x1F0,
    0x170,
};

static const uint64_t compat_control_starts[] = {
    0x3F6,
    0x376,
};

static const uint32_t compat_irqs[] = {
    14,
    15,
};

static void set_ports(fwnode_resource_t* restrict resource, uint64_t start, uint64_t length)
{
    *resource = (fwnode_resource_t) {};
    resource->type = FWNODE_RESOURCE_PORT;
    resource->range.start = start;
    resource->range.length = length;
}

static void describe_compat(ide_channel_t* restrict channel, unsigned number)
{
    set_ports(&channel->resources[0], compat_command_starts[number], 8);
    set_ports(&channel->resources[1], compat_control_starts[number], 1);

    fwnode_resource_t* irq = &channel->resources[2];
    *irq = (fwnode_resource_t) {};
    irq->type = FWNODE_RESOURCE_IRQ;
    irq->irq.number = compat_irqs[number];
    irq->irq.trigger = TRIGGER_EDGE;
    irq->irq.polarity = ACTIVE_HIGH;
    irq->irq.flags = FWNODE_IRQ_ISA;

    channel->fixed.num_resources = 3;
}

// A native channel without an interrupt is still there, and its driver polls
static int describe_native(device_t* restrict device, ide_channel_t* restrict channel, unsigned number)
{
    unsigned bar = number * PCI_IDE_BARS_PER_CHANNEL;
    fwnode_resource_t control;

    int retval = device_get_range(device, bar, &channel->resources[0]);

    if(retval == 0) {
        retval = device_get_range(device, bar + 1, &control);
    }

    if(retval < 0) {
        return retval;
    }

    set_ports(&channel->resources[1], control.range.start + NATIVE_CONTROL_OFFSET, 1);
    channel->resources[1].type = control.type;
    channel->fixed.num_resources = 2;

    fwnode_resource_t* irq = &channel->resources[2];
    *irq = (fwnode_resource_t) {};
    irq->type = FWNODE_RESOURCE_IRQ;

    if(device_get_irq(device, 0, &irq->irq) == 0) {
        channel->fixed.num_resources = 3;
    }

    channel->native = true;

    return 0;
}

static void add_channel(device_t* restrict device, ide_channel_t* restrict channel, unsigned number)
{
    pci_function_t* function = pci_function_of(device);

    snprintf(channel->name, sizeof(channel->name), "ide:%u", number);

    channel->fixed.name = channel->name;
    channel->fixed.id = CHANNEL_ID;
    channel->fixed.resources = channel->resources;

    if(pci_ide_is_compat(function, number)) {
        describe_compat(channel, number);
    }
    else if(describe_native(device, channel, number) < 0) {
        pr_verbose("pci_ide: %s %s: no registers", device->name, channel->name);
        return;
    }

    fwnode_t node;
    fwnode_init_fixed(&node, &channel->fixed);

    device_t* child;
    int retval = device_add(device, &node, channel->name, channel, &child);

    if(retval < 0) {
        pr_warning("pci_ide: %s %s: %s", device->name, channel->name, strerror(retval));
        return;
    }

    device_decref(child);
}

// The firmware may describe the channels too, as ACPI does with
// _ADR 0 and 1; the bus has made nodes of its own for them
static int pci_ide_place_child(device_t* restrict bus, const fwnode_t* restrict child)
{
    return 0;
}

// PCI Local Bus 3.0, 6.2.2: Interrupt Disable masks the pin, which both native channels share
static void set_interrupt_disable(device_t* restrict bus, bool disable)
{
    pci_function_t* function = pci_function_of(bus);
    uint16_t command = pci_read_config16(function, PCI_CFG_COMMAND) & ~(uint16_t)(PCI_CMD_INT_DISABLE);

    if(disable) {
        command |= PCI_CMD_INT_DISABLE;
    }

    pci_write_config16(function, PCI_CFG_COMMAND, command);
}

static void pci_ide_enable_irq(device_t* restrict bus, device_t* restrict child, unsigned index)
{
    ide_controller_t* controller = bus->ctxdata;
    const ide_channel_t* channel = child->bus_data;

    if(channel->native) {
        controller->native_irqs += 1;
        set_interrupt_disable(bus, false);
    }
}

static void pci_ide_disable_irq(device_t* restrict bus, device_t* restrict child, unsigned index)
{
    ide_controller_t* controller = bus->ctxdata;
    const ide_channel_t* channel = child->bus_data;

    if(channel->native) {
        controller->native_irqs -= 1;
        set_interrupt_disable(bus, controller->native_irqs == 0);
    }
}

static const bus_ops_t pci_ide_bus_ops = {
    .place_child = &pci_ide_place_child,
    .enable_irq = &pci_ide_enable_irq,
    .disable_irq = &pci_ide_disable_irq,
};

static int pci_ide_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    return DRIVER_SCORE_DEFAULT;
}

static int pci_ide_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    pci_function_t* function = pci_function_of(device);
    ide_controller_t* controller = kmalloc(sizeof(ide_controller_t));

    if(controller == nullptr) {
        return -ENOMEM;
    }

    memset(controller, 0, sizeof(ide_controller_t));

    device->ctxdata = controller;
    device->bus_ops = &pci_ide_bus_ops;

    // A compatibility channel decodes its ISA ports only while I/O Space is on, which the BARs don't decide
    if(pci_ide_is_compat(function, 0) || pci_ide_is_compat(function, 1)) {
        uint16_t command = pci_read_config16(function, PCI_CFG_COMMAND);
        pci_write_config16(function, PCI_CFG_COMMAND, command | PCI_CMD_IO_SPACE);
    }

    for(unsigned i = 0; i < PCI_IDE_CHANNELS; ++i) {
        add_channel(device, &controller->channels[i], i);
    }

    return 0;
}

// The channels are gone by now, as children go first
static void pci_ide_detach(device_t* restrict device)
{
    device->bus_ops = nullptr;
}

static void pci_ide_release(device_t* restrict device)
{
    kfree(device->ctxdata);
    device->ctxdata = nullptr;
}

static const driver_ops_t pci_ide_ops = {
    .probe = &pci_ide_probe,
    .attach = &pci_ide_attach,
    .detach = &pci_ide_detach,
    .release = &pci_ide_release,
};

static const driver_match_t pci_ide_matches[] = {
    DRIVER_MATCH_PCI_CLASS(PCI_CLASS_STORAGE, PCI_SUBCLASS_IDE),
    DRIVER_MATCH_END(),
};

define_driver(pci_ide, &pci_ide_ops, pci_ide_matches);
