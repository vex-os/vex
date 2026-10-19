#pragma once

#include <vex/spinlock.h>
#include <vex/types.h>

struct thread;
struct listener {
    struct thread* thrd;
    size_t wakeup_index;
};

struct event {
    spinlock_type lock;
    uint32_t num_pending;
    uint32_t num_listeners;
    uint32_t max_listeners;
    struct listener* listeners;
};

void event_init(struct event* restrict evnt);
void event_deinit(struct event* restrict evnt);
ssize_t event_await(struct event** restrict events, size_t count);
ssize_t event_await_blocking(struct event** restrict events, size_t count);
ssize_t event_await_timeout(struct event** restrict events, size_t count, uint64_t timeout_nsec);
ssize_t event_await_interruptible(struct event** restrict events, size_t count);
size_t event_trigger(struct event* restrict evnt, bool linger_pending);
void event_reset(struct event* restrict evnt);
