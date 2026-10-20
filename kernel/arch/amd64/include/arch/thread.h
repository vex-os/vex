#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct hart_context_s hart_context_t;
typedef struct thread_s thread_t;
typedef struct vmspace_s vmspace_t;

typedef struct arch_thread_s arch_thread_t;

struct arch_thread_s {
    void* gs_base;
    void* fs_base;
    uint64_t cr3;
};

void arch_thread_setup_kern(thread_t* restrict thread, void* restrict entry, void* restrict arg);
void arch_thread_setup_user(thread_t* restrict thread, void* restrict entry, void* restrict user_stack_top, vmspace_t* restrict vmspace);
void arch_thread_setup_fork(thread_t* restrict thread, const hart_context_t* restrict ctx, vmspace_t* restrict vmspace);
void arch_thread_exec(thread_t* restrict thread, vmspace_t* restrict vmspace, uintptr_t entry, uintptr_t user_stack_top);
void arch_thread_set_tcb(thread_t* restrict thread, uintptr_t tcb);
void arch_thread_ctx_save(thread_t* restrict thread);
void arch_thread_ctx_load(thread_t* restrict thread);

static __always_inline __nodiscard thread_t* this_thread(void)
{
    thread_t* thread;
    asm volatile("movq %%gs:0, %0" : "=r"(thread));
    return thread;
}
