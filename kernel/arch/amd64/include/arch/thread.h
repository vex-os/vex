#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct thread;
struct vmspace;

struct arch_thread {
    void* gs_base;
    void* fs_base;
    uint64_t cr3;
    void* kernel_stack;
};

static __always_inline __nodiscard struct thread* this_thread(void)
{
    struct thread* thrd;
    asm volatile("movq %%gs:0, %0" : "=r"(thrd));
    return thrd;
}

void arch_thread_setup_kern(struct thread* restrict thrd, void* restrict entry, void* restrict arg, void* restrict kernel_stack_top);
void arch_thread_setup_user(struct thread* restrict thrd, void* restrict entry, void* restrict user_stack_top,
    void* restrict kernel_stack_top, struct vmspace* restrict vms);
void arch_thread_ctx_save(struct thread* restrict thrd);
void arch_thread_ctx_load(struct thread* restrict thrd);
