#include <arch/amd64.gdt.h>
#include <arch/amd64.msr.h>
#include <vex/hart.h>
#include <vex/pmap.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vms.h>

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

    // A kernel thread made from within a system call
    // must not keep that process's address space alive
    thrd->arch.cr3 = sys_pmap.phys;
}

void arch_thread_setup_user(struct thread* restrict thrd, void* restrict entry, void* restrict user_stack_top,
    void* restrict kernel_stack_top, struct vmspace* restrict vms)
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
    thrd->arch.cr3 = vms->pmap->phys;
    thrd->kernel_rsp = kernel_stack_top;
}

void arch_thread_setup_fork(struct thread* restrict thrd, const struct hart_context* restrict ctx, void* restrict kernel_stack_top,
    struct vmspace* restrict vms)
{
    memcpy(&thrd->ctx, ctx, sizeof(thrd->ctx));
    thrd->ctx.syscall_retval = 0;

    // Inside the kernel, userspace's GS base waits in KERNEL_GS_BASE
    thrd->arch.gs_base = (void*)((uintptr_t)(rdmsr(MSR_KERNEL_GS_BASE)));
    thrd->arch.fs_base = (void*)((uintptr_t)(rdmsr(MSR_FS_BASE)));
    thrd->arch.kernel_stack = kernel_stack_top;
    thrd->arch.cr3 = vms->pmap->phys;
    thrd->kernel_rsp = kernel_stack_top;
}

void arch_thread_exec(struct thread* restrict thrd, struct vmspace* restrict vms, uintptr_t entry, uintptr_t user_stack_top)
{
    struct hart_context* ctx = thrd->syscall_ctx;

    memset(ctx, 0, sizeof(struct hart_context));

    ctx->amd64_ds = gdt_selector(GDT_USER_DATA_64, 0, 3);
    ctx->amd64_es = gdt_selector(GDT_USER_DATA_64, 0, 3);
    ctx->amd64_cs = gdt_selector(GDT_USER_CODE_64, 0, 3);
    ctx->amd64_ss = gdt_selector(GDT_USER_DATA_64, 0, 3);
    ctx->amd64_rip = entry;
    ctx->amd64_rsp = user_stack_top;
    ctx->amd64_rflags = 0x202;

    // sysretq takes these instead of
    // the frame's rip, rflags and rsp
    ctx->amd64_rcx = entry;
    ctx->amd64_r11 = 0x202;
    thrd->entry_scratch = (void*)(user_stack_top);

    thrd->arch.gs_base = nullptr;
    thrd->arch.fs_base = nullptr;

    // Switching away in between would
    // save the old CR3 back into the thread
    bool intr_state = disable_interrupts();

    wrmsr(MSR_KERNEL_GS_BASE, 0);
    wrmsr(MSR_FS_BASE, 0);

    thrd->arch.cr3 = vms->pmap->phys;
    pmap_switch(vms->pmap);

    restore_interrupts(intr_state);
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

    wrmsr(MSR_GS_BASE, (uintptr_t)(thrd));
    wrmsr(MSR_FS_BASE, (uintptr_t)(thrd->arch.fs_base));
    wrmsr(MSR_KERNEL_GS_BASE, (uintptr_t)(thrd->arch.gs_base));

    asm volatile("movq %0, %%cr3" ::"r"(thrd->arch.cr3) : "memory");
}
