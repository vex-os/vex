// SPDX-License-Identifier: BSD-3-Clause
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initfn.h>
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

#define HPET_FEMTO_PER_NANO UINT64_C(1000000)
#define HPET_PERIOD_MAX     UINT64_C(100000000)

static volatile uint64_t* hpet_mmio = nullptr;

static inline uint64_t hpet_read_reg(uint32_t offset)
{
    return hpet_mmio[offset / sizeof(uint64_t)];
}

static inline void hpet_write_reg(uint32_t offset, uint64_t value)
{
    hpet_mmio[offset / sizeof(uint64_t)] = value;
}

static uint64_t hpet_op_read(const struct clocksource* restrict clock)
{
    return hpet_read_reg(HPET_REG_MAIN);
}

static const struct clocksource_ops hpet_ops = {
    .read = hpet_op_read,
};

static struct clocksource hpet_clocksource = {
    .ops = &hpet_ops,
    .identity = "hpet",
    .rating = 50,
    .read_mask = UINT64_MAX,
    .read_mult = UINT64_C(0),
    .read_shift = UINT64_C(0),
};

static int init_hpet(void)
{
    uacpi_table table;
    uacpi_status status;
    struct acpi_hpet* hpet;
    void* virt;

    status = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_WARNING, "hpet: ACPI HPET table not found");

        return ENODEV;
    }

    hpet = (struct acpi_hpet*)(table.hdr);

    if(hpet->address.address_space_id) {
        kprintf(KP_WARNING, "hpet: non-memory address space, skipping");

        uacpi_table_unref(&table);

        return ENODEV;
    }

    virt = phys_to_virt(hpet->address.address);

    if(vmm_map(&sys_pagemap, (uintptr_t)(virt), hpet->address.address, VPROT_RW | VPROT_NOCACHE)) {
        kprintf(KP_WARNING, "hpet: failed to map MMIO at %p", (const void*)((uintptr_t)(hpet->address.address)));

        uacpi_table_unref(&table);

        return ENODEV;
    }

    uacpi_table_unref(&table);

    hpet_mmio = (volatile uint64_t*)(virt);

    uint64_t period_fs = hpet_read_reg(HPET_REG_CAPS);
    period_fs >>= HPET_CAPS_PERIOD_SHIFT;

    if(period_fs == 0 || period_fs > HPET_PERIOD_MAX) {
        kprintf(KP_WARNING, "hpet: invalid counter period %llu fs", (unsigned long long)(period_fs));

        return ENODEV;
    }

    hpet_write_reg(HPET_REG_CONFIG, hpet_read_reg(HPET_REG_CONFIG) | HPET_CONFIG_ENABLE);

    hpet_clocksource.read_shift = 32;
    hpet_clocksource.read_mult = period_fs << 32;
    hpet_clocksource.read_mult /= HPET_FEMTO_PER_NANO;

    return register_clocksource(&hpet_clocksource);
}

DEFINE_INITFN(init_hpet, INITFN_STAGE_TIMER, INITFN_ORDER_FIRST);
