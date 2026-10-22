#include <arch/amd64.lapic.h>
#include <vex/device.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/kprintf.h>

static const driver_match_t cpu_matches[] = {
    DRIVER_MATCH_FIRMWARE("ACPI0007"),
    DRIVER_MATCH_END(),
};

static hart_local_t* find_hart(uint64_t cpu_id)
{
    for(unsigned long i = 0; i < num_harts; ++i) {
        if(harts[i].arch.lapic_id == cpu_id) {
            return &harts[i];
        }
    }

    return nullptr;
}

static int cpu_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    return DRIVER_SCORE_DEFAULT;
}

// A CPU without a hart is one the bootloader
// didn't start, which only CPU hotplug can bring up
static int cpu_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    uint64_t cpu_id;

    if(fwnode_get_cpu_id(&device->fwnode, &cpu_id) < 0) {
        return -ENODEV;
    }

    hart_local_t* hart = find_hart(cpu_id);

    if(hart == nullptr) {
        return -ENODEV;
    }

    device->ctxdata = hart;

    return lapic_add_device(device, hart);
}

// UNDONE: move the hart's threads and timers to others
// and take it out of the scheduler; the boot hart stays
// even then, until its duties can be handed over
static int cpu_offline(device_t* restrict device)
{
    hart_local_t* hart = device->ctxdata;

    if(hart == boot_hart()) {
        pr_warning("cpu: %s is the boot CPU, which can't go offline", device->name);
    }
    else {
        pr_warning("cpu: %s can't go offline yet", device->name);
    }

    return -EBUSY;
}

// Never called, as offline always refuses
static void cpu_online(device_t* restrict device)
{
    // empty
}

// Only a gone CPU gets here without going
// offline and the hart then has nothing left to stop
static void cpu_detach(device_t* restrict device)
{
    // empty
}

// The hart in ctxdata isn't the driver's to free
static void cpu_release(device_t* restrict device)
{
    // empty
}

static const driver_ops_t cpu_driver_ops = {
    .probe = &cpu_probe,
    .attach = &cpu_attach,
    .offline = &cpu_offline,
    .online = &cpu_online,
    .detach = &cpu_detach,
    .release = &cpu_release,
};

define_driver(cpu, &cpu_driver_ops, cpu_matches);
