#include <arch/amd64.gdt.h>
#include <arch/amd64.msr.h>
#include <arch/hart.h>
#include <vex/initcall.h>
#include <vex/types.h>

extern void amd64_syscall_entry(void);

static void arch_init_syscall(void)
{
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | MSR_EFER_SCE);

    uint64_t star_value = UINT64_C(0);
    star_value |= ((uint64_t)(gdt_selector(GDT_KERN_DATA_64, 0, 0)) << 48); // STAR[63:48] = kernel data segment selector
    star_value |= ((uint64_t)(gdt_selector(GDT_KERN_CODE_64, 0, 0)) << 32); // STAR[47:32] = kernel code segment selector
    wrmsr(MSR_STAR, star_value);

    wrmsr(MSR_LSTAR, (uintptr_t)(&amd64_syscall_entry));
    wrmsr(MSR_SFMASK, AMD64_RFLAGS_TF | AMD64_RFLAGS_IF | AMD64_RFLAGS_DF | AMD64_RFLAGS_AC);
}

initcall_smp_syscall(arch_init_syscall, INIT_EARLY);
