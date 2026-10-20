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

/// Prepares the saved context so that the thread starts in kernel mode
/// at entry(arg), with interrupts enabled and the kernel's page tables
void arch_thread_setup_kern(thread_t* restrict thread, void* restrict entry, void* restrict arg);

/// Prepares the saved context so that the thread starts in user mode at entry
/// with arg as its first argument, tcb as its thread pointer and a clean FPU state
void arch_thread_setup_user(thread_t* restrict thread, void* restrict entry, void* restrict user_stack_top, uintptr_t arg, uintptr_t tcb,
    vmspace_t* restrict vmspace);

/// Makes the child of a fork resume from the parent's system call
/// with a return value of zero; called by the parent in that call, so
/// the thread pointer and FPU state come from the current hart
/// \param ctx The parent's system call frame
void arch_thread_setup_fork(thread_t* restrict thread, const hart_context_t* restrict ctx, vmspace_t* restrict vmspace);

/// Called by the thread itself in exec: rewrites its system call frame to enter the new image,
/// clears the thread pointer and FPU state, and switches to the new page tables right away
void arch_thread_exec(thread_t* restrict thread, vmspace_t* restrict vmspace, uintptr_t entry, uintptr_t user_stack_top);

/// Sets the calling thread's thread pointer, effective right away
void arch_thread_set_tcb(thread_t* restrict thread, uintptr_t tcb);

/// Called by the scheduler with interrupts disabled around a switch, with the
/// registers already in the saved context; these save and load the rest of the
/// thread's state: thread pointers, page tables, FPU state and, on load, the
/// stack the hart enters the kernel on from user mode
void arch_thread_ctx_save(thread_t* restrict thread);
void arch_thread_ctx_load(thread_t* restrict thread);

static __always_inline __nodiscard thread_t* this_thread(void)
{
    thread_t* thread;
    asm volatile("movq %%gs:0, %0" : "=r"(thread));
    return thread;
}
