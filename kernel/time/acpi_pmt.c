#include <arch/io.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex/clocksrc.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mmio.h>
#include <vex/panic.h>
#include <vex/vprot.h>

#define ACPI_PMT_FREQ_HZ UINT64_C(3579545)

typedef struct acpi_pmt_privdata_s acpi_pmt_privdata_t;

struct acpi_pmt_privdata_s {
    volatile uint32_t* mmio;
    uintptr_t pmio;
};

static uint64_t acpi_pmt_read(clock_source_t* restrict source)
{
    acpi_pmt_privdata_t* priv = source->ctxdata;

    if(priv->mmio) {
        return priv->mmio[0];
    }
    else {
        return port_read32(priv->pmio);
    }
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
    acpi_pmt_privdata_t* priv = kmalloc(sizeof(acpi_pmt_privdata_t));

    if(priv == nullptr) {
        pr_warning("acpi_pmt: out of memory");
        return;
    }

    priv->mmio = nullptr;
    priv->pmio = 0;

    if(fadt->x_pm_tmr_blk.address) {
        if(fadt->x_pm_tmr_blk.address_space_id == ACPI_AS_ID_SYS_MEM) {
            priv->mmio = mmio_map(fadt->x_pm_tmr_blk.address, sizeof(uint32_t), VPROT_MMIO);

            if(priv->mmio == nullptr) {
                pr_warning("acpi_pmt: failed to map the timer");

                kfree(priv);

                return;
            }
        }
        else {
            priv->pmio = fadt->x_pm_tmr_blk.address;
        }
    }
    else if(fadt->pm_tmr_blk) {
        priv->pmio = fadt->pm_tmr_blk;
    }
    else {
        uacpi_table_unref(&table);

        kfree(priv);

        return;
    }

    if(fadt->flags & ACPI_TMR_VAL_EXT) {
        acpi_pmt_clock_source.mask = CLOCK_SOURCE_MASK_32;
    }
    else {
        acpi_pmt_clock_source.mask = CLOCK_SOURCE_MASK_24;
    }

    acpi_pmt_clock_source.ctxdata = priv;

    clock_source_add_hz(&acpi_pmt_clock_source, ACPI_PMT_FREQ_HZ);

    uacpi_table_unref(&table);
}

initcall_bsp_timer(init_acpi_pmt, INIT_FIRST);
