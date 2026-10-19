#pragma once

#include <arch/hart.h>
#include <arch/thread.h>
#include <vex/compiler.h>
#include <vex/spinlock.h>
#include <vex/types.h>

struct credentials;
struct event;
struct process;
struct hart_local;
struct runqueue;

enum {
    THREAD_WORK_EXIT = 1U << 0,
};

enum {
    THREAD_RUNNABLE,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD,
};

struct thread {
    struct thread* self;

    // Pinned, register-free scratch slots for syscall/trap entry trampolines:
    // reachable via thread-pointer-register-relative addressing (%gs on amd64)
    // before any general purpose register is free to use. kernel_rsp is where
    // the kernel stack starts for ring 3 entries; entry_scratch is a transient holding
    // cell for the interrupted user stack pointer while it is being switched away from.
    void* kernel_rsp;
    void* entry_scratch;

    pid_t tid;
    uint16_t state; // Under sched_lock
    bool need_resched;
    bool wait_interrupt;
    uint32_t preempt_count; // Spinlocks held, the thread is not preempted while nonzero

    spinlock_type sched_lock;
    spinlock_type hart_lock; // Held while the thread is on a hart
    spinlock_type yield_await;

    struct hart_context* syscall_ctx;

    struct hart_local* last_hart;  // The hart this thread runs on or last to be running on
    struct hart_local* bound_hart; // The hart this thread MUST run on, nullptr otherwise

    struct runqueue* rq;
    struct thread* rq_next;
    struct thread* rq_prev;

    struct event** events;
    uint32_t wakeup_index;
    uint32_t num_events;
    uint32_t max_events;
    uint32_t pending_work; // THREAD_WORK_*, looked at on every return to ring 3

    uint64_t wake_deadline;
    struct thread* wake_next;

    uint64_t timeslice;

    struct process* proc;
    struct credentials* cred;
    struct thread* tnext;
    struct thread* tprev;

    void* kstack_top;

    struct arch_thread arch;
};

static_assert((0 * sizeof(uintptr_t)) == offsetof(struct thread, self));
static_assert((1 * sizeof(uintptr_t)) == offsetof(struct thread, kernel_rsp));
static_assert((2 * sizeof(uintptr_t)) == offsetof(struct thread, entry_scratch));

// The saved context lives at the very top of the kernel stack, where nothing
// running on that stack reaches, so the thread itself stays small
#define THREAD_CTX_RESERVE align_ceil(sizeof(struct hart_context), 16)

static __always_inline __nodiscard struct hart_context* thread_ctx(const struct thread* restrict thrd)
{
    return (struct hart_context*)((uintptr_t)(thrd->kstack_top) - THREAD_CTX_RESERVE);
}

void exit_to_user(struct hart_context* restrict ctx);
