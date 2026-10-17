#include <vex/cpucaps.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/types.h>

#define CR4_SMEP UINT64_C(0x0000000000100000)
#define CR4_SMAP UINT64_C(0x0000000000200000)

static void arch_init_cpuprot(void)
{
    const struct hart_local* hart = this_hart();

    uint64_t cr4;
    asm volatile("movq %%cr4, %0" : "=r"(cr4));

    if(cpucaps_has(&hart->caps, CPU_FEATURE_SMEP)) {
        cr4 |= CR4_SMEP;
    }

    if(cpucaps_has(&hart->caps, CPU_FEATURE_SMAP)) {
        cr4 |= CR4_SMAP;
    }

    asm volatile("movq %0, %%cr4" ::"r"(cr4) : "memory");
}

initcall_smp_arch(arch_init_cpuprot, INIT_MIDDLE);
