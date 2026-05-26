// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/lapic.h>
#include <amd64/msr.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <mm/hhdm.h>
#include <mm/page.h>
#include <mm/vmm.h>

// IA32_APIC_BASE MSR: bits [35:12] = physical base, bit 11 = global enable
#define MSR_IA32_APIC_BASE     UINT32_C(0x0000001B)
#define MSR_APIC_BASE_PHYSMASK UINT64_C(0x0000000FFFFFF000)
#define MSR_APIC_BASE_ENABLE   UINT64_C(0x0000000000000800)

// SVR: bit 8 = APIC software enable, bits [7:0] = spurious vector
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
    uint64_t lapic_base_msr;
    uintptr_t lapic_phys;

    lapic_base_msr = msr_read(MSR_IA32_APIC_BASE);

    if(UINT64_C(0) == (lapic_base_msr & MSR_APIC_BASE_ENABLE)) {
        panic("local APIC is globally disabled");
    }

    lapic_phys = lapic_base_msr & MSR_APIC_BASE_PHYSMASK;
    lapic_virtual_base = phys_to_virt(lapic_phys);

    if(vmm_map(&sys_pagemap, (uintptr_t)(lapic_virtual_base), lapic_phys, VPROT_RW | VPROT_NOCACHE)) {
        panic("failed to map local APIC MMIO");
    }

    lapic_write(LAPIC_REG_SVR, lapic_read(LAPIC_REG_SVR) | LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);

    kprintf(KP_INFORM, "lapic: id=%u base=0x%016lx\n", lapic_id(), (unsigned long)lapic_phys);

    return 0;
}

DEFINE_INITFN(AMD64_init_lapic, INITFN_STAGE_HART, INITFN_ORDER_FIRST);
