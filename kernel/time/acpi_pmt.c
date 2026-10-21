#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex/acpi.h>
#include <vex/clocksrc.h>
#include <vex/initcall.h>
#include <vex/io.h>
#include <vex/kprintf.h>
#include <vex/string.h>

#define ACPI_PMT_FREQ_HZ UINT64_C(3579545)

static io_region_t timer_register;

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
        return;
    }

    clock_source_add_hz(&acpi_pmt_clock_source, ACPI_PMT_FREQ_HZ);
}

initcall_bsp_timer(init_acpi_pmt, INIT_FIRST);
