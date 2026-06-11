#include <vex.x86/lapic.h>
#include <vex.x86/msr.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/string.h>
#include <vex/vmm.h>

#define MSR_IA32_APIC_BASE     UINT32_C(0x0000001B)
#define MSR_APIC_BASE_PHYSMASK UINT64_C(0x0000000FFFFFF000)
#define MSR_APIC_BASE_ENABLE   UINT64_C(0x0000000000000800)

#define LAPIC_SVR_ENABLE      UINT32_C(0x00000100)
#define LAPIC_SPURIOUS_VECTOR UINT32_C(0x000000FF)

static volatile uint32_t* lapic_mmio = nullptr;

uint32_t lapic_read(uint32_t reg)
{
    return lapic_mmio[reg];
}

void lapic_write(uint32_t reg, uint32_t value)
{
    lapic_mmio[reg] = value;
}

void lapic_broadcast_ipi(uint8_t vector)
{
    if(lapic_mmio) {
        while(lapic_read(LAPIC_REG_ICR_LO) & UINT32_C(0x00001000)) {
            hart_spin();
        }

        lapic_write(LAPIC_REG_ICR_HI, UINT32_C(0));
        lapic_write(LAPIC_REG_ICR_LO, vector | UINT32_C(0x000C4000));
    }
}

static void arch_init_lapic(void)
{
    uint64_t msr = rdmsr(MSR_IA32_APIC_BASE);

    if(0 == (msr & MSR_APIC_BASE_ENABLE)) {
        panic("local apic is globally disabled");
    }

    uintptr_t lapic_phys = msr & MSR_APIC_BASE_PHYSMASK;
    lapic_mmio = phys_to_virt(lapic_phys);

    int retval = vmm_map(&sys_pagemap, (uintptr_t)(lapic_mmio), lapic_phys, VPROT_MMIO);

    if(retval < 0 && retval != -EEXIST) {
        panic("lapic: vmm_map: %s", strerror(retval));
    }

    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    svr |= LAPIC_SVR_ENABLE;
    svr |= LAPIC_SPURIOUS_VECTOR;

    lapic_write(LAPIC_REG_SVR, svr);
}

initcall_smp_ireq(arch_init_lapic, INIT_FIRST);
