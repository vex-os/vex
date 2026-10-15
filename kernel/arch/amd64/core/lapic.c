#include <arch/amd64.lapic.h>
#include <arch/amd64.msr.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmap.h>
#include <vex/string.h>

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

void lapic_ipi(uint32_t target, uint32_t mode, uint8_t vector)
{
    if(lapic_mmio) {
        bool intr_state = disable_interrupts(); // An IPI sent from an interrupt handler in between would overwrite ICR_HI

        while(lapic_read(LAPIC_REG_ICR_LO) & UINT32_C(0x00001000)) {
            hart_spin();
        }

        lapic_write(LAPIC_REG_ICR_HI, target << 24);
        lapic_write(LAPIC_REG_ICR_LO, vector | mode);

        restore_interrupts(intr_state);
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

    int retval = pmap_map(&sys_pmap, (uintptr_t)(lapic_mmio), lapic_phys, VPROT_MMIO);

    if(retval < 0 && retval != -EEXIST) {
        panic("lapic: pmap_map: %s", strerror(retval));
    }

    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    svr |= LAPIC_SVR_ENABLE;
    svr |= LAPIC_SPURIOUS_VECTOR;

    lapic_write(LAPIC_REG_SVR, svr);
}

initcall_smp_ireq(arch_init_lapic, INIT_FIRST);
