// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/lapic.h>
#include <amd64/msr.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initcall.h>
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

define_initcall(amd64_lapic, INITCALL_STAGE_SMP_IREQ, INITCALL_ORDER_FIRST)
{
    int retval;
    uint32_t lapic_svr_value;
    uint64_t lapic_base_msr;
    uintptr_t lapic_phys;

    lapic_base_msr = msr_read(MSR_IA32_APIC_BASE);
    strict_kassert_text(lapic_base_msr & MSR_APIC_BASE_ENABLE, "local apic is globally disabled");

    lapic_phys = lapic_base_msr & MSR_APIC_BASE_PHYSMASK;
    lapic_virtual_base = phys_to_virt(lapic_phys);

    retval = vmm_map(&sys_pagemap, (uintptr_t)(lapic_virtual_base), lapic_phys, VPROT_RW | VPROT_NOCACHE);
    strict_kassert_text(retval == 0 || retval == EEXIST, "vmm_map failed");

    lapic_svr_value = lapic_read(LAPIC_REG_SVR);
    lapic_svr_value |= LAPIC_SVR_ENABLE;
    lapic_svr_value |= LAPIC_SPURIOUS_VECTOR;

    lapic_write(LAPIC_REG_SVR, lapic_svr_value);

    pr_inform("lapic: id=%u, base=%p", (unsigned)(lapic_query_id()), (const void*)(lapic_phys));
}
