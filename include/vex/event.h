#ifndef BD9665B7_59DD_4197_BB60_936735C86008
#define BD9665B7_59DD_4197_BB60_936735C86008
#pragma once

#include <vex/spinlock.h>
#include <vex/types.h>

#define MAX_LISTENERS 32

struct thread;
struct listener {
    struct thread* thrd;
    size_t event_index;
};

struct event {
    spinlock_type lock;
    size_t num_pending;
    size_t num_listeners;
    struct listener listeners[MAX_LISTENERS];
};

ssize_t event_await(struct event** restrict events, size_t count);
ssize_t event_await_blocking(struct event** restrict events, size_t count);
size_t event_trigger(struct event* restrict evnt, bool linger_pending);

#endif /* BD9665B7_59DD_4197_BB60_936735C86008 */
