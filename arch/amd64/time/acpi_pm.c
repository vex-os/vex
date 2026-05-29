// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/pmio.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <mm/hhdm.h>
#include <mm/malloc.h>
#include <mm/vmm.h>
#include <mm/vprot.h>
#include <time/clocksource.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#define ACPI_PM_PERIOD_FS UINT64_C(279365238) // 1.0e15 / 3579545 Hz

struct acpi_pm_privdata {
    volatile uint32_t* mmio;
    uintptr_t pmio;
};

static uint64_t acpi_pm_op_read_nsec(struct clocksource* restrict clock)
{
    struct acpi_pm_privdata* priv = clock->privdata;

    if(priv->mmio) {
        return timecounter_update(&clock->tc, priv->mmio[0]);
    }
    else {
        return timecounter_update(&clock->tc, pmread32(priv->pmio));
    }
}

static const struct clocksource_ops acpi_pm_ops = {
    .read_nsec = &acpi_pm_op_read_nsec,
};

static struct clocksource acpi_pm_clocksource = {
    .ops = &acpi_pm_ops,
    .identifier = "acpi_pm",
    .rating = 100,
};

define_initcall(amd64_acpi_pm_clocksource, INITCALL_STAGE_BSP_TIMER, INITCALL_ORDER_FIRST)
{
    int retval;
    uacpi_table table;
    uacpi_status status;
    struct acpi_fadt* fadt;
    struct acpi_pm_privdata* priv;
    void* virt;

    status = uacpi_table_find_by_signature(ACPI_FADT_SIGNATURE, &table);

    if(uacpi_likely_success(status)) {
        fadt = (struct acpi_fadt*)(table.hdr);

        priv = kmalloc(sizeof(struct acpi_pm_privdata));
        strict_kassert_text(priv, "out of memory");

        priv->mmio = nullptr;
        priv->pmio = 0;

        if(fadt->x_pm_tmr_blk.address) {
            if(fadt->x_pm_tmr_blk.address_space_id == ACPI_AS_ID_SYS_MEM) {
                virt = phys_to_virt(fadt->x_pm_tmr_blk.address);

                retval = vmm_map(&sys_pagemap, (uintptr_t)(virt), fadt->x_pm_tmr_blk.address, VPROT_RW | VPROT_NOCACHE);
                strict_kassert_text(retval == 0 || retval == EEXIST, "mmio map failed");

                priv->mmio = virt;
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
            acpi_pm_clocksource.tc.calc_mask = UINT32_MAX;
        }
        else {
            acpi_pm_clocksource.tc.calc_mask = UINT64_C(0x00FFFFFF);
        }

        acpi_pm_clocksource.privdata = priv;

        if(priv->mmio) {
            timecounter_init(&acpi_pm_clocksource.tc, ACPI_PM_PERIOD_FS, priv->mmio[0]);
        }
        else {
            timecounter_init(&acpi_pm_clocksource.tc, ACPI_PM_PERIOD_FS, pmread32(priv->pmio));
        }

        uacpi_table_unref(&table);

        register_clocksource(&acpi_pm_clocksource);
    }
}
