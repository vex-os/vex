#include <sys.amd64/tss.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys.arch/sched.h>
#include <sys/page.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/vmm.h>

void __noreturn uthread_exit(struct uthread* restrict thread)
{
    struct hart_local* hart = this_hart();

    disable_interrupts();

    hart->ut_curr = nullptr;
    hart->ut_dead = thread;
    hart->kt_curr = hart->kt_idle;
    hart->kt_idle->state = THREAD_EXEC;

    vmm_switch(&sys_pagemap);

    asm volatile("movq %0, %%rsp; sti; 1: call sched_preempt; jmp 1b" ::"r"(hart->amd64_exit_ksp) : "memory");
    unreachable();
}

uintptr_t __used uthread_prepare_switch(struct uthread* restrict next)
{
    struct hart_local* hart = this_hart();

    uintptr_t kstack_top = PAGE_SIZE + page_align(next->kstack);
    hart->amd64_tssp->rsp[0] = kstack_top;
    hart->amd64_syscall_ksp = kstack_top;

    vmm_switch(next->owner->vm);

    return next->kstack;
}
