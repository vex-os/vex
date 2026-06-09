#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex.amd64/pmio.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/malloc.h>
#include <vex/panic.h>
#include <vex/string.h>
#include <vex/vmm.h>
#include <vex/vprot.h>

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

static void arch_init_acpi_clocksrc(void)
{
    uacpi_table table;
    uacpi_status status = uacpi_table_find_by_signature(ACPI_FADT_SIGNATURE, &table);

    if(uacpi_unlikely_error(status)) {
        return;
    }

    struct acpi_fadt* fadt = (struct acpi_fadt*)(table.hdr);
    struct acpi_clocksrc_privdata* priv = kmalloc(sizeof(struct acpi_clocksrc_privdata));

    if(priv == nullptr) {
        pr_warning("acpi_clocksrc: out of memory");
        return;
    }

    priv->mmio = nullptr;
    priv->pmio = 0;

    if(fadt->x_pm_tmr_blk.address) {
        if(fadt->x_pm_tmr_blk.address_space_id == ACPI_AS_ID_SYS_MEM) {
            priv->mmio = phys_to_virt(fadt->x_pm_tmr_blk.address);

            int retval = vmm_map(&sys_pagemap, (uintptr_t)(priv->mmio), fadt->x_pm_tmr_blk.address, VPROT_MMIO);

            if(retval < 0 && retval != -EEXIST) {
                pr_warning("acpi_clocksrc: vmm_map failed: %s", strerror(retval));

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

    add_clocksrc_hz(&acpi_clocksrc, ACPI_CLOCKSRC_FREQ_HZ);

    uacpi_table_unref(&table);
}

initcall_bsp_timer(arch_init_acpi_clocksrc, INIT_FIRST);
