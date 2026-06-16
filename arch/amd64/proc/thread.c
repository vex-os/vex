#include <arch/amd64.gdt.h>
#include <arch/amd64.msr.h>
#include <vex/hart.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vmm.h>

void arch_thread_setup_kern(struct thread* restrict thrd, void* restrict entry, void* restrict arg, void* restrict kernel_stack_top)
{
    memset(&thrd->ctx, 0, sizeof(thrd->ctx));

    thrd->ctx.amd64_ds = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.amd64_es = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.amd64_cs = gdt_selector(GDT_KERN_CODE_64, 0, 0);
    thrd->ctx.amd64_ss = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    thrd->ctx.amd64_rip = (uintptr_t)(entry);
    thrd->ctx.amd64_rsp = (uintptr_t)(kernel_stack_top);
    thrd->ctx.amd64_rdi = (uintptr_t)(arg);
    thrd->ctx.amd64_rflags = 0x202;

    thrd->arch.gs_base = thrd;
    thrd->arch.fs_base = nullptr;
    thrd->arch.kernel_stack = kernel_stack_top;
    thrd->kernel_rsp = kernel_stack_top;

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    thrd->arch.cr3 = cr3;
}

void arch_thread_setup_user(struct thread* restrict thrd, void* restrict entry, void* restrict user_stack_top,
    void* restrict kernel_stack_top, void* restrict pfault_stack_top, struct pagemap* restrict vm)
{
    memset(&thrd->ctx, 0, sizeof(thrd->ctx));

    thrd->ctx.amd64_ds = gdt_selector(GDT_USER_DATA_64, 0, 3);
    thrd->ctx.amd64_es = gdt_selector(GDT_USER_DATA_64, 0, 3);
    thrd->ctx.amd64_cs = gdt_selector(GDT_USER_CODE_64, 0, 3);
    thrd->ctx.amd64_ss = gdt_selector(GDT_USER_DATA_64, 0, 3);
    thrd->ctx.amd64_rip = (uintptr_t)(entry);
    thrd->ctx.amd64_rsp = (uintptr_t)(user_stack_top);
    thrd->ctx.amd64_rflags = 0x202;

    thrd->arch.gs_base = nullptr;
    thrd->arch.fs_base = nullptr;
    thrd->arch.kernel_stack = kernel_stack_top;
    thrd->arch.pfault_stack = pfault_stack_top;
    thrd->arch.cr3 = vm->phys;
    thrd->kernel_rsp = kernel_stack_top;
}

void arch_thread_ctx_save(struct thread* restrict thrd)
{
    // I'm pretty certain arch_thread_ctx_save isn't going to get
    // called from userspace so whatever userspace keeps in GS_BASE
    // is swapped into KERNEL_GS_BASE so it's preserved here
    thrd->arch.gs_base = (void*)((uintptr_t)(rdmsr(MSR_KERNEL_GS_BASE)));
    thrd->arch.fs_base = (void*)((uintptr_t)(rdmsr(MSR_FS_BASE)));

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    thrd->arch.cr3 = cr3;
}

void arch_thread_ctx_load(struct thread* restrict thrd)
{
    struct hart_local* hart = this_hart();

    hart->arch.tss.rsp_0 = (uintptr_t)(thrd->arch.kernel_stack);
    hart->arch.tss.ist_2 = (uintptr_t)(thrd->arch.pfault_stack);

    wrmsr(MSR_GS_BASE, (uintptr_t)(thrd));
    wrmsr(MSR_FS_BASE, (uintptr_t)(thrd->arch.fs_base));
    wrmsr(MSR_KERNEL_GS_BASE, (uintptr_t)(thrd->arch.gs_base));

    asm volatile("movq %0, %%cr3" ::"r"(thrd->arch.cr3) : "memory");
}
