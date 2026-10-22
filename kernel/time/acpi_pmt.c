#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex/acpi.h>
#include <vex/clocksrc.h>
#include <vex/device.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/io.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/string.h>

#define ACPI_PMT_FREQ_HZ UINT64_C(3579545)

static io_region_t timer_register; // Its length is zero while no timer counts

static uint64_t acpi_pmt_read(clock_source_t* restrict source)
{
    return io_read32(&timer_register, 0);
}

static const clock_source_ops_t acpi_pmt_ops = {
    .read = &acpi_pmt_read,
};

static clock_source_t acpi_pmt_clock_source = {
    .ops = &acpi_pmt_ops,
    .name = "acpi_pm_timer",
    .rating = 100,
};

// Time goes on from another clock source, so the timer can go
static int acpi_pmt_offline(device_t* restrict device)
{
    if(clock_source_remove(&acpi_pmt_clock_source) == -EBUSY) {
        pr_warning("acpi_pmt: %s is the only clock source", device->name);
        return -EBUSY;
    }

    return 0;
}

static void acpi_pmt_online(device_t* restrict device)
{
    clock_source_add_hz(&acpi_pmt_clock_source, ACPI_PMT_FREQ_HZ);
}

// A timer that stopped still beats time going back to zero, so the only source stays
static void acpi_pmt_detach(device_t* restrict device)
{
    if(device_is_gone(device) && clock_source_remove(&acpi_pmt_clock_source) == -EBUSY) {
        pr_warning("acpi_pmt: %s is gone, but time has nothing else to go on", device->name);
    }
}

// The framework has unmapped a register in memory by now
static void acpi_pmt_release(device_t* restrict device)
{
    if(device->ctxdata) {
        timer_register = (io_region_t) {};
    }
}

static const driver_ops_t acpi_pmt_driver_ops = {
    .offline = &acpi_pmt_offline,
    .online = &acpi_pmt_online,
    .detach = &acpi_pmt_detach,
    .release = &acpi_pmt_release,
};

static const driver_match_t acpi_pmt_matches[] = {
    DRIVER_MATCH_END(),
};

// Not in the drivers' table, as the FADT describes the timer rather than the namespace
static const driver_t acpi_pmt_driver = {
    .name = "acpi_pmt",
    .ops = &acpi_pmt_driver_ops,
    .matches = acpi_pmt_matches,
};

static void init_acpi_pmt(void)
{
    uacpi_table table;
    uacpi_status status = uacpi_table_find_by_signature(ACPI_FADT_SIGNATURE, &table);

    if(uacpi_unlikely_error(status)) {
        return;
    }

    struct acpi_fadt* fadt = (struct acpi_fadt*)(table.hdr);
    struct acpi_gas gas = fadt->x_pm_tmr_blk;

    // Before ACPI 2.0 the block is in ports, and the FADT only has its address
    if(gas.address == 0) {
        memset(&gas, 0, sizeof(gas));
        gas.address_space_id = ACPI_AS_ID_SYS_IO;
        gas.address = fadt->pm_tmr_blk;
    }

    if(fadt->flags & ACPI_TMR_VAL_EXT) {
        acpi_pmt_clock_source.mask = CLOCK_SOURCE_MASK_32;
    }
    else {
        acpi_pmt_clock_source.mask = CLOCK_SOURCE_MASK_24;
    }

    uacpi_table_unref(&table);

    if(gas.address == 0) {
        return;
    }

    int retval = acpi_gas_region(&gas, sizeof(uint32_t), &timer_register);

    if(retval < 0) {
        pr_warning("acpi_pmt: %s", strerror(retval));
        timer_register = (io_region_t) {};
        return;
    }

    clock_source_add_hz(&acpi_pmt_clock_source, ACPI_PMT_FREQ_HZ);
}

initcall_bsp_timer(init_acpi_pmt, INIT_FIRST);

// After the firmware's root node is there, and before the tree is bound
static void init_acpi_pmt_node(void)
{
    if(timer_register.length == 0) {
        return;
    }

    device_t* root = device_get_root();

    if(root == nullptr) {
        panic("acpi_pmt: out of memory");
    }

    device_t* device;

    if(device_add_bound(root, nullptr, "acpi_pm_timer", &acpi_pmt_driver, &acpi_pmt_clock_source, &device) < 0) {
        panic("acpi_pmt: out of memory");
    }

    if(timer_register.space == IO_SPACE_MEMORY && device_adopt_io(device, &timer_register) < 0) {
        panic("acpi_pmt: out of memory");
    }

    device_decref(device);
    device_decref(root);
}

initcall_bsp_bus(init_acpi_pmt_node, INIT_LATE);
