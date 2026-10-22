#include <uacpi/uacpi.h>
#include <vex/device.h>
#include <vex/pci.h>

static const driver_match_t host_bridge_matches[] = {
    DRIVER_MATCH_FW("PNP0A03"),
    DRIVER_MATCH_FW("PNP0A08"),
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

static int host_bridge_probe(device_t* restrict device, const driver_match_t* restrict match, unsigned* restrict bid)
{
    *bid = DRIVER_BID_DEFAULT;

    return 0;
}

static int host_bridge_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    unsigned segment = evaluate_number(&device->fwnode, "_SEG");
    unsigned bus = evaluate_number(&device->fwnode, "_BBN");

    return pci_root_attach(device, segment, bus);
}

static const driver_ops_t host_bridge_ops = {
    .probe = &host_bridge_probe,
    .attach = &host_bridge_attach,
};

define_driver(pci_host_bridge, &host_bridge_ops, host_bridge_matches);
