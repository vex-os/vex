// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/lapic.h>
#include <amd64/msr.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <mm/hhdm.h>
#include <mm/page.h>
#include <mm/vmm.h>

#define MSR_IA32_APIC_BASE     UINT32_C(0x0000001B)
#define MSR_APIC_BASE_PHYSMASK UINT64_C(0x0000000FFFFFF000)
#define MSR_APIC_BASE_ENABLE   UINT64_C(0x0000000000000800)

#define LAPIC_SVR_ENABLE      UINT32_C(0x00000100)
#define LAPIC_SPURIOUS_VECTOR UINT32_C(0x000000FF)

static volatile uint32_t* lapic_virtual_base = nullptr;

uint32_t lapic_read(uint32_t reg)
{
    return lapic_virtual_base[reg];
}

void lapic_write(uint32_t reg, uint32_t value)
{
    lapic_virtual_base[reg] = value;
}

static int AMD64_init_lapic(void)
{
    uint32_t lapic_svr_value;
    uint64_t lapic_base_msr;
    uintptr_t lapic_phys;

    lapic_base_msr = msr_read(MSR_IA32_APIC_BASE);

    if(UINT64_C(0) == (lapic_base_msr & MSR_APIC_BASE_ENABLE)) {
        panic("lapic: local apic is globally disabled");
    }

    lapic_phys = lapic_base_msr & MSR_APIC_BASE_PHYSMASK;
    lapic_virtual_base = phys_to_virt(lapic_phys);

    if(vmm_map(&sys_pagemap, (uintptr_t)(lapic_virtual_base), lapic_phys, VPROT_RW | VPROT_NOCACHE)) {
        panic("lapic: failed to map local apic mmio");
    }

    lapic_svr_value = lapic_read(LAPIC_REG_SVR);
    lapic_svr_value |= LAPIC_SVR_ENABLE;
    lapic_svr_value |= LAPIC_SPURIOUS_VECTOR;

    lapic_write(LAPIC_REG_SVR, lapic_svr_value);

    kprintf(KP_INFORM, "lapic: id=%u, base=%p", (unsigned)(lapic_query_id()), (const void*)(lapic_phys));

    return 0;
}

DEFINE_INITFN_MP(AMD64_init_lapic, INITFN_STAGE_MP_IREQ, INITFN_ORDER_FIRST);
