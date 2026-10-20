#pragma once

#include <vex/spinlock.h>
#include <vex/types.h>

typedef struct thread_s thread_t;

typedef struct event_listener_s event_listener_t;
typedef struct event_s event_t;

struct event_listener_s {
    thread_t* thread;
    size_t wakeup_index;
};

struct event_s {
    spinlock_t lock;
    uint32_t num_pending;
    uint32_t num_listeners;
    uint32_t max_listeners;
    event_listener_t* listeners;
};

void event_init(event_t* restrict event);
void event_deinit(event_t* restrict event);
ssize_t event_await(event_t** restrict events, size_t count);
ssize_t event_await_blocking(event_t** restrict events, size_t count);
ssize_t event_await_timeout(event_t** restrict events, size_t count, uint64_t timeout_nsec);
ssize_t event_await_interruptible(event_t** restrict events, size_t count);
size_t event_trigger(event_t* restrict event, bool linger_pending);
void event_reset(event_t* restrict event);
