#include <vex/device.h>
#include <vex/driver.h>
#include <vex/errno.h>

#include "isa_internal.h"

static const driver_match_t bridge_matches[] = {
    DRIVER_MATCH_PCI_CLASS(0x06, 0x01),
    DRIVER_MATCH_END(),
};

// UNDONE: a bridge the firmware doesn't describe, such
// as one on a card, needs its devices found through ISA PnP
static int bridge_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    if(device->fwnode.ops == nullptr) {
        return -ENODEV;
    }

    return DRIVER_SCORE_DEFAULT;
}

static int bridge_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    device->bus_ops = &isa_bus_ops;

    return 0;
}

// The devices on the bus are gone by now, as children go first
static void bridge_detach(device_t* restrict device)
{
    device->bus_ops = nullptr;
}

static void bridge_release(device_t* restrict device)
{
    // empty
}

static const driver_ops_t bridge_ops = {
    .probe = &bridge_probe,
    .attach = &bridge_attach,
    .detach = &bridge_detach,
    .release = &bridge_release,
};

define_driver(isa_bridge, &bridge_ops, bridge_matches);
