#include <vex/device.h>
#include <vex/device_bus.h>
#include <vex/errno.h>
#include <vex/fwnode.h>
#include <vex/kprintf.h>
#include <vex/sprintf.h>
#include <vex/string.h>

#include "isa_internal.h"

#define ISA_ID_MAX 16

typedef struct first_range_s first_range_t;

struct first_range_s {
    uint64_t start;
    bool found;
};

static void find_first_range(const fwnode_resource_t* resource, void* arg)
{
    first_range_t* first = arg;
    bool is_range = resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;

    if(first->found || !is_range) {
        return;
    }

    first->start = resource->range.start;
    first->found = true;
}

// A device is named after its first register range on the bus, as isa:3f8,
// and only one without registers falls back to the firmware's name for it
static int make_name(const fwnode_t* restrict node, char* restrict name, size_t size)
{
    first_range_t first = {};
    fwnode_for_each_resource(node, &find_first_range, &first);

    if(!first.found) {
        return fwnode_get_name(node, name, size);
    }

    unsigned long long start = first.start;
    snprintf(name, size, "isa:%llx", start);

    return 0;
}

// A child without an id is no ISA device, but some
// other part of the chipset the bridge stands for
static int isa_place_child(device_t* restrict bridge, const fwnode_t* restrict child)
{
    char id[ISA_ID_MAX];
    char name[DEVICE_NAME_MAX];

    if(fwnode_get_id(child, id, sizeof(id)) < 0) {
        return -ENOENT;
    }

    if(make_name(child, name, sizeof(name)) < 0) {
        return 0;
    }

    device_t* device = nullptr;
    int retval = device_add(bridge, child, name, nullptr, &device);

    if(retval < 0) {
        pr_warning("isa: %s: %s", name, strerror(retval));
        return 0;
    }

    device_decref(device);

    return 0;
}

const bus_ops_t isa_bus_ops = {
    .place_child = &isa_place_child,
};
