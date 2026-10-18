#pragma once

#include <arch/hart.h>
#include <arch/thread.h>
#include <vex/spinlock.h>
#include <vex/types.h>

struct credentials;
struct event;
struct process;
struct hart_local;
struct runqueue;

enum {
    THREAD_RUNNABLE,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD,
};

struct thread {
    struct thread* self;
    uint64_t errno_value;

    // Pinned, register-free scratch slots for syscall/trap entry trampolines:
    // reachable via thread-pointer-register-relative addressing (%gs on amd64)
    // before any general purpose register is free to use. kernel_rsp mirrors
    // arch.kernel_stack; entry_scratch is a transient holding cell for the
    // interrupted user stack pointer while it is being switched away from.
    void* kernel_rsp;
    void* entry_scratch;

    unsigned long tid;
    unsigned short state; // Under sched_lock

    struct hart_context* syscall_ctx;

    spinlock_type sched_lock;
    spinlock_type hart_lock; // Held while the thread is on a hart
    spinlock_type yield_await;
    struct hart_local* last_hart;  // The hart this thread runs on or last to be running on
    struct hart_local* bound_hart; // The hart this thread MUST run on, nullptr otherwise

    struct runqueue* rq;
    struct thread* rq_next;
    struct thread* rq_prev;

    struct event** events;
    size_t wakeup_index;
    size_t num_events;
    size_t max_events;

    uint64_t wake_deadline;
    struct thread* wake_next;

    uint64_t timeslice;
    unsigned preempt_count; // Spinlocks held, the thread is not preempted while nonzero
    bool need_resched;

    struct process* proc;
    struct credentials* cred;
    struct thread* tnext;
    struct thread* tprev;

    void* kstack_top;

    struct hart_context ctx;
    struct arch_thread arch;
};

static_assert((0 * sizeof(uintptr_t)) == offsetof(struct thread, self));
static_assert((1 * sizeof(uintptr_t)) == offsetof(struct thread, errno_value));
static_assert((2 * sizeof(uintptr_t)) == offsetof(struct thread, kernel_rsp));
static_assert((3 * sizeof(uintptr_t)) == offsetof(struct thread, entry_scratch));
