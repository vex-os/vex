#include <arch/amd64.msr.h>
#include <arch/pmap.h>
#include <vex/initcall.h>

static void program_pat(void)
{
    if(AMD64_PAT_VALUE == rdmsr(MSR_PAT)) {
        return;
    }

    asm volatile("wbinvd" ::: "memory");
    wrmsr(MSR_PAT, AMD64_PAT_VALUE);
    pmap_flush_all();
}

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
