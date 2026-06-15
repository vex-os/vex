#ifndef FFDED8C1_DEB6_4CC0_8403_D765FAABCD58
#define FFDED8C1_DEB6_4CC0_8403_D765FAABCD58
#pragma once

#include <vex/spinlock.h>

enum {
    THREAD_RUNNABLE,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD,
};

struct process;
struct thread {
    struct thread* self;
    uint64_t errno_value;

    unsigned long tid;
    unsigned short state;

    spinlock_type hart_lock;
    spinlock_type yield_await;
    struct hart_local* hart;
    unsigned short hart_index;

    bool enqueued;
    bool enqueued_by_signal;

    struct process* owner;
    struct hart_context ctx;
    uint64_t timeslice;

    void* x86_gs_base;
    void* x86_fs_base;
    uint64_t x86_cr3;
    void* x86_kernel_stack;
    void* x86_pfault_stack;
};

static_assert(0 == offsetof(struct thread, self));
static_assert(8 == offsetof(struct thread, errno_value));

static __always_inline __nodiscard struct thread* this_thread(void)
{
    struct thread* thrd;
    asm volatile("movq %%gs:0, %0" : "=r"(thrd));
    return thrd;
}

void arch_thread_setup_kern(struct thread* restrict thrd, void* restrict entry, void* restrict arg, void* restrict stack_top);
void arch_thread_ctx_save(struct thread* restrict thrd);
void arch_thread_ctx_load(struct thread* restrict thrd);

#endif /* FFDED8C1_DEB6_4CC0_8403_D765FAABCD58 */
