#include <uacpi/uacpi.h>
#include <vex/firmware.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/pci.h>
#include <vex/string.h>

static const char* const host_bridge_ids[] = {
    "PNP0A03",
    "PNP0A08",
    nullptr,
};

// ACPI 6.5, 6.5.5 _BBN and 6.5.6 _SEG: either is zero when the bridge has none
static unsigned evaluate_number(const firmware_node_t* restrict node, const char* restrict name)
{
    uacpi_u64 value;

    if(uacpi_unlikely_error(uacpi_eval_simple_integer(node->handle, name, &value))) {
        return 0;
    }

    return value;
}

static void add_host_bridge(const firmware_node_t* restrict node)
{
    unsigned segment = evaluate_number(node, "_SEG");
    unsigned bus = evaluate_number(node, "_BBN");
    int retval = pci_root_add(node, segment, bus);

    if(retval < 0) {
        pr_warning("acpi: host bridge %04X:%02X: %s", segment, bus, strerror(retval));
    }
}

static void visit_node(const firmware_node_t* node, void* arg)
{
    if(!firmware_node_matches(node, host_bridge_ids)) {
        firmware_node_for_each_child(node, &visit_node, nullptr);
        return;
    }

    if(firmware_node_is_present(node)) {
        add_host_bridge(node);
    }
}

static void init_pci_root(void)
{
    firmware_node_t root;

    if(firmware_root(&root) < 0) {
        return;
    }

    firmware_node_for_each_child(&root, &visit_node, nullptr);
}

initcall_bsp_bus(init_pci_root, INIT_LATE);
