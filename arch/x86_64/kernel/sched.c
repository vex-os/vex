#include <vex.x86/gdt.h>
#include <vex.x86/msr.h>
#include <vex/hart.h>
#include <vex/sched.h>
#include <vex/string.h>

#define MSR_FS_BASE 0xC0000100
#define MSR_GS_BASE 0xC0000101

void arch_thread_setup_kern(struct thread* restrict thrd, void* restrict entry, void* restrict arg, void* restrict stack_top)
{
    memset(&thrd->ctx, 0, sizeof(thrd->ctx));

    thrd->ctx.x86_ds = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.x86_es = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.x86_cs = gdt_selector(GDT_KERN_CODE_64, 0, 0);
    thrd->ctx.x86_ss = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.x86_rip = (uintptr_t)(entry);
    thrd->ctx.x86_rsp = (uintptr_t)(stack_top);
    thrd->ctx.x86_rdi = (uintptr_t)(arg);
    thrd->ctx.x86_rflags = 0x202;

    thrd->x86_gs_base = thrd;
    thrd->x86_fs_base = nullptr;
    thrd->x86_kernel_stack = stack_top;

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    thrd->x86_cr3 = cr3;
}

void arch_thread_ctx_save(struct thread* restrict thrd)
{
    thrd->x86_gs_base = (void*)((uintptr_t)(rdmsr(MSR_GS_BASE)));
    thrd->x86_fs_base = (void*)((uintptr_t)(rdmsr(MSR_FS_BASE)));

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    thrd->x86_cr3 = cr3;
}

void arch_thread_ctx_load(struct thread* restrict thrd)
{
    struct hart_local* hart = this_hart();

    hart->x86_tss.rsp_0 = (uintptr_t)(thrd->x86_kernel_stack);

    wrmsr(MSR_GS_BASE, (uintptr_t)(thrd->x86_gs_base));
    wrmsr(MSR_FS_BASE, (uintptr_t)(thrd->x86_fs_base));

    asm volatile("movq %0, %%cr3" ::"r"(thrd->x86_cr3) : "memory");
}
