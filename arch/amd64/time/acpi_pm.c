// SPDX-License-Identifier: BSD-3-Clause
#include <amd64/pmio.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <mm/hhdm.h>
#include <mm/slab.h>
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
    .identity = "acpi_pm",
    .rating = 100,
};

static int AMD64_init_acpi_pm(void)
{
    uacpi_table table;
    uacpi_status status;
    struct acpi_fadt* fadt;
    struct acpi_pm_privdata* priv;
    void* virt;

    status = uacpi_table_find_by_signature(ACPI_FADT_SIGNATURE, &table);

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_WARNING, "acpi_pm: acpi fadt is not present");
        return ENODEV;
    }

    fadt = (struct acpi_fadt*)(table.hdr);

    priv = slab_alloc(sizeof(struct acpi_pm_privdata));

    if(priv == nullptr) {
        uacpi_table_unref(&table);
        return ENOMEM;
    }

    priv->mmio = nullptr;
    priv->pmio = 0;

    if(fadt->x_pm_tmr_blk.address) {
        if(fadt->x_pm_tmr_blk.address_space_id == ACPI_AS_ID_SYS_MEM) {
            virt = phys_to_virt(fadt->x_pm_tmr_blk.address);

            if(vmm_map(&sys_pagemap, (uintptr_t)(virt), fadt->x_pm_tmr_blk.address, VPROT_RW | VPROT_NOCACHE)) {
                kprintf(KP_WARNING, "acpi_pm: failed to map mmio at %p", (const void*)((uintptr_t)(fadt->x_pm_tmr_blk.address)));
                uacpi_table_unref(&table);
                slab_free(priv);
                return ENODEV;
            }

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
        kprintf(KP_WARNING, "acpi_pm: acpi pm timer not present");
        uacpi_table_unref(&table);
        slab_free(priv);
        return ENODEV;
    }

    if(fadt->flags & ACPI_TMR_VAL_EXT) {
        acpi_pm_clocksource.tc.calc_mask = UINT32_MAX;
    }
    else {
        acpi_pm_clocksource.tc.calc_mask = UINT64_C(0x00FFFFFF);
    }

    acpi_pm_clocksource.privdata = priv;

    uacpi_table_unref(&table);

    if(priv->mmio) {
        timecounter_init(&acpi_pm_clocksource.tc, ACPI_PM_PERIOD_FS, priv->mmio[0]);
    }
    else {
        timecounter_init(&acpi_pm_clocksource.tc, ACPI_PM_PERIOD_FS, pmread32(priv->pmio));
    }

    return register_clocksource(&acpi_pm_clocksource);
}

DEFINE_INITFN(AMD64_init_acpi_pm, INITFN_STAGE_TIMER, INITFN_ORDER_FIRST);
