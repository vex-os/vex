#include <amd64/pmio.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <mm/hhdm.h>
#include <mm/malloc.h>
#include <mm/vmm.h>
#include <mm/vprot.h>
#include <time/clocksrc.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#define ACPI_CLOCKSRC_FREQ_HZ UINT64_C(3579545)

struct acpi_clocksrc_privdata {
    volatile uint32_t* mmio;
    uintptr_t pmio;
};

static uint64_t acpi_clocksrc_read(struct clocksrc* restrict src)
{
    struct acpi_clocksrc_privdata* priv = src->ctxdata;

    if(priv->mmio) {
        return priv->mmio[0];
    }
    else {
        return pmread32(priv->pmio);
    }
}

static const struct clocksrc_ops acpi_clocksrc_ops = {
    .read = &acpi_clocksrc_read,
};

static struct clocksrc acpi_clocksrc = {
    .ops = &acpi_clocksrc_ops,
    .identifier = "acpi_pm_timer",
    .rating = 100,
};

define_initcall(amd64_acpi_clocksrc, INITCALL_STAGE_BSP_TIMER, INITCALL_ORDER_FIRST)
{
    uacpi_table table;
    uacpi_status status = uacpi_table_find_by_signature(ACPI_FADT_SIGNATURE, &table);

    if(uacpi_unlikely_error(status)) {
        return;
    }

    struct acpi_fadt* fadt = (struct acpi_fadt*)(table.hdr);
    struct acpi_clocksrc_privdata* priv = kmalloc(sizeof(struct acpi_clocksrc_privdata));

    if(priv == nullptr) {
        pr_warning("acpi: out of memory");
        return;
    }

    priv->mmio = nullptr;
    priv->pmio = 0;

    if(fadt->x_pm_tmr_blk.address) {
        if(fadt->x_pm_tmr_blk.address_space_id == ACPI_AS_ID_SYS_MEM) {
            priv->mmio = phys_to_virt(fadt->x_pm_tmr_blk.address);

            int retval = vmm_map(&sys_pagemap, (uintptr_t)(priv->mmio), fadt->x_pm_tmr_blk.address, VPROT_MMIO);

            if(retval < 0 && retval != -EEXIST) {
                pr_warning("acpi: vmm_map failed: %s", strerror(retval));

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
        acpi_clocksrc.mask = CLOCKSRC_MASK_32;
    }
    else {
        acpi_clocksrc.mask = CLOCKSRC_MASK_24;
    }

    acpi_clocksrc.ctxdata = priv;

    clocksrc_register_hz(&acpi_clocksrc, ACPI_CLOCKSRC_FREQ_HZ);

    pr_notice("acpi: PM-timer registered (%s)", priv->mmio ? "mmio" : "pmio");

    uacpi_table_unref(&table);
}
