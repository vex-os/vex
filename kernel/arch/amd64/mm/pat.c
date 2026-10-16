#include <arch/amd64.cpuid.h>
#include <arch/amd64.msr.h>
#include <arch/pmap.h>
#include <vex/initcall.h>
#include <vex/panic.h>

static void program_pat(void)
{
    if(AMD64_PAT_VALUE == rdmsr(MSR_PAT)) {
        return;
    }

    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    if(0 == (edx & CPUID_1_EDX_PAT)) {
        panic("pat: not supported by the processor");
    }

    asm volatile("wbinvd" ::: "memory");
    wrmsr(MSR_PAT, AMD64_PAT_VALUE);
    pmap_flush_all();
}

// The boot hart runs the per-hart initcalls
// only at the multicore stage, after the first
// mappings that may want write-combining
static void arch_init_pat_bsp(void)
{
    program_pat();
}

static void arch_init_pat(void)
{
    program_pat();
}

initcall_bsp_arch(arch_init_pat_bsp, INIT_EARLY);
initcall_smp_arch(arch_init_pat, INIT_EARLY);
