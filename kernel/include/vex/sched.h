#pragma once

#include <arch/sched.h>
#include <vex/types.h>

struct hart_context;
struct thread;
struct process;

typedef void (*thread_entry_kern)(void* restrict arg);

struct thread* create_thread_kern(thread_entry_kern entry, void* restrict arg);
struct thread* create_thread_kern_enq(thread_entry_kern entry, void* restrict arg);
struct thread* create_thread_user(struct process* restrict proc, uintptr_t entry, uintptr_t stack_top);
struct thread* create_thread_user_enq(struct process* restrict proc, uintptr_t entry, uintptr_t stack_top);
struct thread* create_thread_fork(struct process* restrict proc, const struct hart_context* restrict ctx);

void sched_enqueue(struct thread* restrict thrd);
void sched_dequeue(struct thread* restrict thrd);
void sched_wake_at(struct thread* restrict thrd, uint64_t deadline);
void sched_wake_cancel(struct thread* restrict thrd);
void sched_terminate(void) __noreturn;
void sched_yield(void);
