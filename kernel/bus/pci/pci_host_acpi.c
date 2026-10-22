#include <uacpi/uacpi.h>
#include <vex/device.h>
#include <vex/driver.h>
#include <vex/pci.h>

static const driver_match_t host_bridge_matches[] = {
    DRIVER_MATCH_FIRMWARE("PNP0A03"),
    DRIVER_MATCH_FIRMWARE("PNP0A08"),
    DRIVER_MATCH_END(),
};

// ACPI 6.5, 6.5.5 _BBN and 6.5.6 _SEG: either is zero when the bridge has none
static unsigned evaluate_number(const fwnode_t* restrict node, const char* restrict name)
{
    uacpi_u64 value;

    if(uacpi_unlikely_error(uacpi_eval_simple_integer(node->handle, name, &value))) {
        return 0;
    }

    return value;
}

static int host_bridge_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    return DRIVER_SCORE_DEFAULT;
}

static int host_bridge_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    unsigned segment = evaluate_number(&device->fwnode, "_SEG");
    unsigned bus = evaluate_number(&device->fwnode, "_BBN");

    return pci_root_attach(device, segment, bus);
}

// The functions below are gone by now, as children go first
static void host_bridge_detach(device_t* restrict device)
{
    pci_root_detach(device);
}

static void host_bridge_release(device_t* restrict device)
{
    // empty
}

static const driver_ops_t host_bridge_ops = {
    .probe = &host_bridge_probe,
    .attach = &host_bridge_attach,
    .detach = &host_bridge_detach,
    .release = &host_bridge_release,
};

define_driver(pci_host_bridge, &host_bridge_ops, host_bridge_matches);
