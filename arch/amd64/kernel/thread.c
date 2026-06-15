#include <vex.amd64/gdt.h>
#include <vex.amd64/msr.h>
#include <vex/hart.h>
#include <vex/string.h>
#include <vex/thread.h>

#define MSR_FS_BASE 0xC0000100
#define MSR_GS_BASE 0xC0000101

void arch_thread_setup_kern(struct thread* restrict thrd, void* restrict entry, void* restrict arg, void* restrict stack_top)
{
    memset(&thrd->ctx, 0, sizeof(thrd->ctx));

    thrd->ctx.amd64_ds = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.amd64_es = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.amd64_cs = gdt_selector(GDT_KERN_CODE_64, 0, 0);
    thrd->ctx.amd64_ss = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.amd64_rip = (uintptr_t)(entry);
    thrd->ctx.amd64_rsp = (uintptr_t)(stack_top);
    thrd->ctx.amd64_rdi = (uintptr_t)(arg);
    thrd->ctx.amd64_rflags = 0x202;

    thrd->arch.gs_base = thrd;
    thrd->arch.fs_base = nullptr;
    thrd->arch.kernel_stack = stack_top;

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    thrd->arch.cr3 = cr3;
}

void arch_thread_ctx_save(struct thread* restrict thrd)
{
    thrd->arch.gs_base = (void*)((uintptr_t)(rdmsr(MSR_GS_BASE)));
    thrd->arch.fs_base = (void*)((uintptr_t)(rdmsr(MSR_FS_BASE)));

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    thrd->arch.cr3 = cr3;
}

void arch_thread_ctx_load(struct thread* restrict thrd)
{
    struct hart_local* hart = this_hart();

    hart->arch.tss.rsp_0 = (uintptr_t)(thrd->arch.kernel_stack);

    wrmsr(MSR_GS_BASE, (uintptr_t)(thrd->arch.gs_base));
    wrmsr(MSR_FS_BASE, (uintptr_t)(thrd->arch.fs_base));

    asm volatile("movq %0, %%cr3" ::"r"(thrd->arch.cr3) : "memory");
}
