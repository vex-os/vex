#ifndef C7E3A2B1_4F9D_4E8C_B6A5_D2C3E4F5A6B7
#define C7E3A2B1_4F9D_4E8C_B6A5_D2C3E4F5A6B7
#pragma once

#include <vex.arch/hart.h>
#include <vex.arch/thread.h>
#include <vex/spinlock.h>
#include <vex/types.h>

#ifndef MAX_EVENTS
#define MAX_EVENTS 32
#endif

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

    unsigned long tid;
    unsigned short state;

    spinlock_type hart_lock;
    spinlock_type yield_await;
    struct hart_local* hart;
    unsigned short hart_index;

    bool enqueued;
    bool enqueued_by_signal;
    struct event* events[MAX_EVENTS];
    size_t event_index;
    size_t num_events;

    struct process* owner;
    struct hart_context ctx;
    uint64_t timeslice;

    struct arch_thread arch;
};

static_assert((0 * sizeof(uintptr_t)) == offsetof(struct thread, self));
static_assert((1 * sizeof(uintptr_t)) == offsetof(struct thread, errno_value));

#endif /* C7E3A2B1_4F9D_4E8C_B6A5_D2C3E4F5A6B7 */
