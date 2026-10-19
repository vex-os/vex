#include <arch/amd64.fpu.h>
#include <arch/amd64.gdt.h>
#include <arch/amd64.msr.h>
#include <vex/hart.h>
#include <vex/pmap.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vms.h>

static void* fpu_state_of(const struct thread* restrict thrd)
{
    if(thrd->proc == nullptr) {
        return nullptr;
    }

    uintptr_t ctx = (uintptr_t)(thread_ctx(thrd));

    return (void*)(align_floor(ctx - fpu_state_size, FPU_STATE_ALIGN));
}

void arch_thread_setup_kern(struct thread* restrict thrd, void* restrict entry, void* restrict arg)
{
    struct hart_context* ctx = thread_ctx(thrd);

    memset(ctx, 0, sizeof(struct hart_context));

    ctx->amd64_ds = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    ctx->amd64_es = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    ctx->amd64_cs = gdt_selector(GDT_KERN_CODE_64, 0, 0);
    ctx->amd64_ss = gdt_selector(GDT_KERN_DATA_64, 0, 0);
    ctx->amd64_rip = (uintptr_t)(entry);
    ctx->amd64_rsp = (uintptr_t)(ctx);
    ctx->amd64_rdi = (uintptr_t)(arg);
    ctx->amd64_rflags = AMD64_RFLAGS_FIXED | AMD64_RFLAGS_IF;

    thrd->arch.gs_base = thrd;
    thrd->arch.fs_base = nullptr;
    thrd->kernel_rsp = ctx;

    // A kernel thread made from within a system call
    // must not keep that process's address space alive
    thrd->arch.cr3 = sys_pmap.phys;
}

void arch_thread_setup_user(struct thread* restrict thrd, void* restrict entry, void* restrict user_stack_top, struct vmspace* restrict vms)
{
    struct hart_context* ctx = thread_ctx(thrd);
    void* fpu_state = fpu_state_of(thrd);

    fpu_init_state(fpu_state);

    memset(ctx, 0, sizeof(struct hart_context));

    ctx->amd64_ds = gdt_selector(GDT_USER_DATA_64, 0, 3);
    ctx->amd64_es = gdt_selector(GDT_USER_DATA_64, 0, 3);
    ctx->amd64_cs = gdt_selector(GDT_USER_CODE_64, 0, 3);
    ctx->amd64_ss = gdt_selector(GDT_USER_DATA_64, 0, 3);
    ctx->amd64_rip = (uintptr_t)(entry);
    ctx->amd64_rsp = (uintptr_t)(user_stack_top);
    ctx->amd64_rflags = AMD64_RFLAGS_FIXED | AMD64_RFLAGS_IF;

    thrd->arch.gs_base = nullptr;
    thrd->arch.fs_base = nullptr;
    thrd->arch.cr3 = vms->pmap->phys;
    thrd->kernel_rsp = fpu_state;
}

void arch_thread_setup_fork(struct thread* restrict thrd, const struct hart_context* restrict ctx, struct vmspace* restrict vms)
{
    struct hart_context* child_ctx = thread_ctx(thrd);
    void* fpu_state = fpu_state_of(thrd);

    fpu_save(fpu_state);

    memcpy(child_ctx, ctx, sizeof(struct hart_context));
    child_ctx->syscall_retval = 0;

    // Inside the kernel, userspace's GS base waits in KERNEL_GS_BASE
    thrd->arch.gs_base = (void*)((uintptr_t)(rdmsr(MSR_KERNEL_GS_BASE)));
    thrd->arch.fs_base = (void*)((uintptr_t)(rdmsr(MSR_FS_BASE)));
    thrd->arch.cr3 = vms->pmap->phys;
    thrd->kernel_rsp = fpu_state;
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
    ctx->amd64_rflags = AMD64_RFLAGS_FIXED | AMD64_RFLAGS_IF;

    // sysretq takes these instead of
    // the frame's rip, rflags and rsp
    ctx->amd64_rcx = entry;
    ctx->amd64_r11 = AMD64_RFLAGS_FIXED | AMD64_RFLAGS_IF;
    thrd->entry_scratch = (void*)(user_stack_top);

    thrd->arch.gs_base = nullptr;
    thrd->arch.fs_base = nullptr;

    // Switching away in between would
    // save the old CR3 back into the thread
    bool intr_state = disable_interrupts();

    wrmsr(MSR_KERNEL_GS_BASE, 0);
    wrmsr(MSR_FS_BASE, 0);
    fpu_reset();

    thrd->arch.cr3 = vms->pmap->phys;
    pmap_switch(vms->pmap);

    restore_interrupts(intr_state);
}

void arch_thread_set_tcb(struct thread* restrict thrd, uintptr_t tcb)
{
    bool intr_state = disable_interrupts();

    thrd->arch.fs_base = (void*)(tcb);
    wrmsr(MSR_FS_BASE, tcb);

    restore_interrupts(intr_state);
}

void arch_thread_ctx_save(struct thread* restrict thrd)
{
    thrd->arch.gs_base = (void*)((uintptr_t)(rdmsr(MSR_KERNEL_GS_BASE)));
    thrd->arch.fs_base = (void*)((uintptr_t)(rdmsr(MSR_FS_BASE)));

    uint64_t cr3;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    thrd->arch.cr3 = cr3;

    void* fpu_state = fpu_state_of(thrd);

    if(fpu_state) {
        fpu_save(fpu_state);
    }
}

void arch_thread_ctx_load(struct thread* restrict thrd)
{
    struct hart_local* hart = this_hart();

    hart->arch.tss.rsp_0 = (uintptr_t)(thrd->kernel_rsp);

    wrmsr(MSR_GS_BASE, (uintptr_t)(thrd));
    wrmsr(MSR_FS_BASE, (uintptr_t)(thrd->arch.fs_base));
    wrmsr(MSR_KERNEL_GS_BASE, (uintptr_t)(thrd->arch.gs_base));

    asm volatile("movq %0, %%cr3" ::"r"(thrd->arch.cr3) : "memory");

    void* fpu_state = fpu_state_of(thrd);

    if(fpu_state) {
        fpu_load(fpu_state);
    }
}
