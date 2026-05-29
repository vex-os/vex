// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <mm/hhdm.h>
#include <mm/vmm.h>
#include <mm/vprot.h>
#include <time/clocksource.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#define HPET_REG_CAPS   0x000
#define HPET_REG_CONFIG 0x010
#define HPET_REG_MAIN   0x0F0

#define HPET_CAPS_PERIOD_SHIFT UINT64_C(32)
#define HPET_CONFIG_ENABLE     UINT64_C(1)
#define HPET_PERIOD_MAX        UINT64_C(100000000)

static inline uint64_t hpet_read_reg(volatile uint64_t* restrict mmio, uint32_t offset)
{
    return mmio[offset / sizeof(uint64_t)];
}

static inline void hpet_write_reg(volatile uint64_t* restrict mmio, uint32_t offset, uint64_t value)
{
    mmio[offset / sizeof(uint64_t)] = value;
}

static uint64_t hpet_op_read_nsec(struct clocksource* restrict clock)
{
    volatile uint64_t* mmio = clock->privdata;

    return timecounter_update((struct timecounter*)(&clock->tc), hpet_read_reg(mmio, HPET_REG_MAIN));
}

static const struct clocksource_ops hpet_ops = {
    .read_nsec = hpet_op_read_nsec,
};

static struct clocksource hpet_clocksource = {
    .ops = &hpet_ops,
    .identifier = "hpet",
    .rating = 200,
};

define_initcall(amd64_hpet_clocksource, INITCALL_STAGE_BSP_TIMER, INITCALL_ORDER_FIRST)
{
    int retval;
    uacpi_table table;
    uacpi_status status;
    struct acpi_hpet* hpet;
    uint32_t hpet_config_reg;
    uint64_t hpet_period_fsec;
    volatile uint64_t* mmio;

    status = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);

    if(uacpi_likely_success(status)) {
        hpet = (struct acpi_hpet*)(table.hdr);

        if(hpet->address.address) {
            mmio = phys_to_virt(hpet->address.address);

            retval = vmm_map(&sys_pagemap, (uintptr_t)(mmio), hpet->address.address, VPROT_RW | VPROT_NOCACHE);
            strict_kassert_text(retval == 0 || retval == EEXIST, "mmio map failed");

            hpet_clocksource.privdata = (void*)(mmio); // this removes the volatile qualifier without a warning

            uacpi_table_unref(&table);

            hpet_period_fsec = hpet_read_reg(mmio, HPET_REG_CAPS);
            hpet_period_fsec >>= HPET_CAPS_PERIOD_SHIFT;

            if(hpet_period_fsec == 0 || hpet_period_fsec > HPET_PERIOD_MAX) {
                pr_warning("hpet: invalid counter period %llu fs", (unsigned long long)(hpet_period_fsec));
                return;
            }

            hpet_config_reg = hpet_read_reg(mmio, HPET_REG_CONFIG);
            hpet_config_reg |= HPET_CONFIG_ENABLE;

            hpet_write_reg(mmio, HPET_REG_CONFIG, hpet_config_reg);

            hpet_clocksource.tc.calc_mask = UINT64_MAX;

            timecounter_init(&hpet_clocksource.tc, hpet_period_fsec, hpet_read_reg(mmio, HPET_REG_MAIN));

            register_clocksource(&hpet_clocksource);
        }
    }
}
