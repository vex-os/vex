#include <uacpi/uacpi.h>
#include <vex/device.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/pci.h>
#include <vex/string.h>
#include <vex/sysbus.h>

static const char* const host_bridge_ids[] = {
    "PNP0A03",
    "PNP0A08",
    nullptr,
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

static int host_bridge_probe(sysbus_device_t* restrict sysbus_device)
{
    device_t* device = sysbus_device->priv.device;
    unsigned segment = evaluate_number(&device->fwnode, "_SEG");
    unsigned bus = evaluate_number(&device->fwnode, "_BBN");
    int retval = pci_root_attach(device, segment, bus);

    if(retval < 0) {
        pr_warning("acpi: host bridge %04X:%02X: %s", segment, bus, strerror(retval));
    }

    return retval;
}

static const sysbus_driver_ops_t host_bridge_ops = {
    .probe = &host_bridge_probe,
};

static sysbus_driver_t host_bridge_driver = {
    .ops = &host_bridge_ops,
    .ids = host_bridge_ids,
};

static void init_pci_root(void)
{
    sysbus_driver_add(&host_bridge_driver);
}

initcall_bsp_bus(init_pci_root, INIT_LATE);
