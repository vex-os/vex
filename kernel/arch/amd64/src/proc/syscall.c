#include <arch/amd64.gdt.h>
#include <arch/amd64.msr.h>
#include <vex/initcall.h>
#include <vex/types.h>

extern void amd64_syscall_entry(void);

static void arch_init_syscall(void)
{
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | UINT64_C(0x0000000000000001)); // EFER.SCE

    uint64_t star_value = UINT64_C(0);
    star_value |= ((uint64_t)(gdt_selector(GDT_KERN_DATA_64, 0, 0)) << 48); // STAR[63:48] = kernel data segment selector
    star_value |= ((uint64_t)(gdt_selector(GDT_KERN_CODE_64, 0, 0)) << 32); // STAR[47:32] = kernel code segment selector
    wrmsr(MSR_STAR, star_value);

    wrmsr(MSR_LSTAR, (uintptr_t)(&amd64_syscall_entry));
    wrmsr(MSR_SFMASK, UINT64_C(0x0000000000000700)); // Clear (TF | IF | DF) on syscall entry
}

initcall_smp_syscall(arch_init_syscall, INIT_EARLY);
