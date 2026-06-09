#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/string.h>
#include <vex/vmm.h>
#include <vex/vprot.h>

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

static uint64_t hpet_read(struct clocksrc* restrict src)
{
    volatile uint64_t* mmio = src->ctxdata;

    return hpet_read_reg(mmio, HPET_REG_MAIN);
}

static const struct clocksrc_ops hpet_ops = {
    .read = &hpet_read,
};

static struct clocksrc hpet_clocksrc = {
    .ops = &hpet_ops,
    .identifier = "hpet",
    .rating = 200,
};

static void init_hpet_clocksrc(void)
{
    uacpi_table table;
    uacpi_status status = uacpi_table_find_by_signature(ACPI_HPET_SIGNATURE, &table);

    if(uacpi_likely_success(status)) {
        struct acpi_hpet* hpet = (struct acpi_hpet*)(table.hdr);

        if(hpet->address.address) {
            volatile uint64_t* mmio = phys_to_virt(hpet->address.address);

            int retval = vmm_map(&sys_pagemap, (uintptr_t)(mmio), hpet->address.address, VPROT_MMIO);

            if(retval < 0 && retval != -EEXIST) {
                panic("hpet: vmm_map: %s", strerror(retval));
            }

            hpet_clocksrc.ctxdata = (void*)(mmio); // this removes the volatile qualifier without a warning

            uacpi_table_unref(&table);

            uint64_t hpet_period_fsec = hpet_read_reg(mmio, HPET_REG_CAPS);
            hpet_period_fsec >>= HPET_CAPS_PERIOD_SHIFT;

            if(hpet_period_fsec == 0 || hpet_period_fsec > HPET_PERIOD_MAX) {
                pr_verbose("hpet: invalid counter period %llu fs", (unsigned long long)(hpet_period_fsec));
                return;
            }

            uint32_t hpet_config_reg = hpet_read_reg(mmio, HPET_REG_CONFIG);
            hpet_config_reg |= HPET_CONFIG_ENABLE;

            hpet_write_reg(mmio, HPET_REG_CONFIG, hpet_config_reg);

            hpet_clocksrc.mask = CLOCKSRC_MASK_64;

            add_clocksrc_raw(&hpet_clocksrc, UINT64_C(1), UINT64_C(1000000000000000) / hpet_period_fsec);
        }
    }
}

initcall_bsp_timer(init_hpet_clocksrc, INIT_FIRST);
