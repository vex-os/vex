#include <amd64/pmio.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <mm/hhdm.h>
#include <mm/malloc.h>
#include <mm/vmm.h>
#include <mm/vprot.h>
#include <time/clocksrc.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#define ACPI_PM_FREQ_HZ UINT64_C(3579545)

struct acpipm_privdata {
    volatile uint32_t* mmio;
    uintptr_t pmio;
};

static uint64_t acpipm_op_read(struct clocksrc* restrict src)
{
    struct acpipm_privdata* priv = src->ctxdata;

    if(priv->mmio) {
        return priv->mmio[0];
    }
    else {
        return pmread32(priv->pmio);
    }
}

static const struct clocksrc_ops acpipm_ops = {
    .read = &acpipm_op_read,
};

static struct clocksrc acpipm_clocksrc = {
    .ops = &acpipm_ops,
    .identifier = "acpipm",
    .rating = 100,
};

define_initcall(amd64_acpipm_clocksrc, INITCALL_STAGE_BSP_TIMER, INITCALL_ORDER_FIRST)
{
    int retval;
    uacpi_table table;
    uacpi_status status;
    struct acpi_fadt* fadt;
    struct acpipm_privdata* priv;
    void* virt;

    status = uacpi_table_find_by_signature(ACPI_FADT_SIGNATURE, &table);

    if(uacpi_likely_success(status)) {
        fadt = (struct acpi_fadt*)(table.hdr);

        priv = kmalloc(sizeof(struct acpipm_privdata));
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
            acpipm_clocksrc.mask = UINT32_MAX;
        }
        else {
            acpipm_clocksrc.mask = UINT64_C(0x00FFFFFF);
        }

        acpipm_clocksrc.ctxdata = priv;

        register_clocksrc_hz(&acpipm_clocksrc, ACPI_PM_FREQ_HZ);

        uacpi_table_unref(&table);
    }
}
