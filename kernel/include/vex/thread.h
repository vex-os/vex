#pragma once

#include <arch/hart.h>
#include <arch/thread.h>
#include <vex/spinlock.h>
#include <vex/types.h>

struct event;
struct process;
struct hart_local;

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
    unsigned short state;

    spinlock_type hart_lock;
    spinlock_type yield_await;
    struct hart_local* hart;
    struct hart_local* pinned_hart; // Set before the first enqueue; nullptr runs on any hart

    bool enqueued_by_signal;
    struct thread* rq_next;
    struct thread* rq_prev;

    struct event** events; // events that can wake up this thread
    size_t wakeup_index;
    size_t num_events;
    size_t max_events;

    uint64_t timeslice;
    unsigned preempt_count; // Spinlocks held, the thread is not preempted while nonzero
    bool need_resched;      // A tick arrived while preempt_count was nonzero

    struct process* proc;
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
