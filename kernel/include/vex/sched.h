#pragma once

#include <arch/sched.h>
#include <vex/types.h>

typedef struct hart_context_s hart_context_t;
typedef struct thread_s thread_t;
typedef struct process_s process_t;

typedef void (*kernel_thread_entry_t)(void* restrict arg);

thread_t* thread_create_kernel(kernel_thread_entry_t entry, void* restrict arg);
thread_t* thread_spawn_kernel(kernel_thread_entry_t entry, void* restrict arg);
thread_t* thread_create_user(process_t* restrict process, uintptr_t entry, uintptr_t stack_top);
thread_t* thread_spawn_user(process_t* restrict process, uintptr_t entry, uintptr_t stack_top);
thread_t* thread_create_fork(process_t* restrict process, const hart_context_t* restrict ctx);
thread_t* thread_create_sibling(const thread_t* restrict creator, uintptr_t entry, uintptr_t stack_top, uintptr_t arg, uintptr_t tcb);

void sched_enqueue(thread_t* restrict thread);
void sched_dequeue(thread_t* restrict thread, bool interruptible);
void sched_kick(thread_t* restrict thread);
void sched_wake_at(thread_t* restrict thread, uint64_t deadline);
void sched_wake_cancel(thread_t* restrict thread);
void sched_terminate(void) __noreturn;
void sched_yield(void);
