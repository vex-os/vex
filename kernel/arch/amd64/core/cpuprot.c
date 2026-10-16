#include <arch/amd64.cpuid.h>
#include <vex/initcall.h>
#include <vex/types.h>

#define CR4_SMEP UINT64_C(0x0000000000100000)
#define CR4_SMAP UINT64_C(0x0000000000200000)

// The kernel reaches user memory only through the
// direct map, never through user addresses, so SMAP
// costs nothing and needs no stac/clac around copies
static void arch_init_cpuprot(void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(0, 0, &eax, &ebx, &ecx, &edx);

    if(eax < 7) {
        return;
    }

    cpuid(7, 0, &eax, &ebx, &ecx, &edx);

    uint64_t cr4;
    asm volatile("movq %%cr4, %0" : "=r"(cr4));

    if(ebx & CPUID_7_EBX_SMEP) {
        cr4 |= CR4_SMEP;
    }

    if(ebx & CPUID_7_EBX_SMAP) {
        cr4 |= CR4_SMAP;
    }

    asm volatile("movq %0, %%cr4" ::"r"(cr4) : "memory");
}

initcall_smp_arch(arch_init_cpuprot, INIT_MIDDLE);
