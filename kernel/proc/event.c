#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/hart.h>
#include <vex/kmalloc.h>
#include <vex/panic.h>
#include <vex/sched.h>
#include <vex/slab.h>
#include <vex/thread.h>

static_assert(sizeof(struct event) <= SLAB_MAX_SIZE);

void event_init(struct event* restrict evnt)
{
    evnt->lock = SPINLOCK_INIT;
    evnt->num_pending = 0;
    evnt->num_listeners = 0;
    evnt->max_listeners = 0;
    evnt->listeners = nullptr;
}

void event_deinit(struct event* restrict evnt)
{
    kfree(evnt->listeners);

    evnt->num_pending = 0;
    evnt->num_listeners = 0;
    evnt->max_listeners = 0;
    evnt->listeners = nullptr;
}

ssize_t event_await(struct event** restrict events, size_t count)
{
    bool intr_state = disable_interrupts();

    for(size_t i = 0; i < count; ++i) {
        spinlock_acquire(&events[i]->lock);
    }

    ssize_t result = -EAGAIN;

    for(size_t i = 0; i < count; ++i) {
        if(events[i]->num_pending > 0) {
            events[i]->num_pending -= 1;
            result = i;
            break;
        }
    }

    for(size_t i = 0; i < count; ++i) {
        spinlock_release(&events[i]->lock);
    }

    restore_interrupts(intr_state);

    return result;
}

// A deadline of zero waits forever
static ssize_t await_until(struct event** restrict events, size_t count, uint64_t deadline)
{
    ssize_t result = event_await(events, count);

    if(result >= 0) {
        return result;
    }

    struct thread* thrd = this_thread();

    bool intr_state = disable_interrupts();

    for(size_t i = 0; i < count; ++i) {
        spinlock_acquire(&events[i]->lock);
    }

    // re-check pending under lock to close the race between
    // event_await and acquiring locks here
    for(size_t i = 0; i < count; ++i) {
        if(events[i]->num_pending > 0) {
            events[i]->num_pending -= 1;

            for(size_t j = 0; j < count; ++j) {
                spinlock_release(&events[j]->lock);
            }

            restore_interrupts(intr_state);

            return i;
        }
    }

    if(count > thrd->max_events) {
        struct event** grown = krealloc(thrd->events, count * sizeof(struct event*));

        if(grown == nullptr) {
            for(size_t i = 0; i < count; ++i) {
                spinlock_release(&events[i]->lock);
            }

            restore_interrupts(intr_state);
            return -ENOMEM;
        }

        thrd->events = grown;
        thrd->max_events = count;
    }

    for(size_t i = 0; i < count; ++i) {
        struct event* evnt = events[i];

        if(evnt->num_listeners >= evnt->max_listeners) {
            size_t new_max_listeners = 4;

            if(evnt->max_listeners) {
                new_max_listeners = evnt->max_listeners * 2;
            }

            struct listener* grown = krealloc(evnt->listeners, new_max_listeners * sizeof(struct listener));

            if(grown == nullptr) {
                for(size_t j = 0; j < i; ++j) {
                    struct event* registered = events[j];

                    registered->num_listeners -= 1;

                    for(size_t k = 0; k < registered->num_listeners; ++k) {
                        if(registered->listeners[k].thrd == thrd) {
                            registered->listeners[k] = registered->listeners[registered->num_listeners];
                            break;
                        }
                    }
                }

                for(size_t j = 0; j < count; ++j) {
                    spinlock_release(&events[j]->lock);
                }

                restore_interrupts(intr_state);
                return -ENOMEM;
            }

            evnt->listeners = grown;
            evnt->max_listeners = new_max_listeners;
        }

        struct listener* dest = &evnt->listeners[evnt->num_listeners++];
        dest->wakeup_index = i;
        dest->thrd = thrd;

        thrd->events[i] = evnt;
    }

    thrd->num_events = count;

    sched_dequeue(thrd);

    if(deadline) {
        sched_wake_at(thrd, deadline);
    }

    for(size_t i = 0; i < count; ++i) {
        spinlock_release(&events[i]->lock);
    }

    // sched_yield() busy-waits with interrupts enabled to receive its own
    // wakeup IPI; restore the caller's interrupt state before calling it
    // rather than after, or that wait deadlocks with interrupts left
    // disabled on this hart.
    restore_interrupts(intr_state);

    sched_yield();

    if(deadline) {
        sched_wake_cancel(thrd);
    }

    // A trigger takes every listener off its event,
    // so a thread still listening everywhere timed out
    bool triggered = false;

    for(size_t i = 0; i < thrd->num_events; ++i) {
        struct event* evnt = thrd->events[i];
        bool listening = false;

        spinlock_acquire(&evnt->lock);

        for(size_t j = 0; j < evnt->num_listeners; ++j) {
            if(evnt->listeners[j].thrd == thrd) {
                evnt->num_listeners -= 1;
                evnt->listeners[j] = evnt->listeners[evnt->num_listeners];
                listening = true;
                break;
            }
        }

        spinlock_release(&evnt->lock);

        if(!listening) {
            triggered = true;
        }
    }

    thrd->num_events = 0;

    if(thrd->enqueued_by_signal) {
        return -EINTR;
    }

    if(!triggered) {
        return -ETIMEDOUT;
    }

    return thrd->wakeup_index;
}

ssize_t event_await_blocking(struct event** restrict events, size_t count)
{
    return await_until(events, count, 0);
}

ssize_t event_await_timeout(struct event** restrict events, size_t count, uint64_t timeout_nsec)
{
    uint64_t deadline = clocksrc_nsec() + timeout_nsec;

    if(deadline < timeout_nsec || deadline == 0) {
        deadline = UINT64_MAX;
    }

    return await_until(events, count, deadline);
}

size_t event_trigger(struct event* restrict evnt, bool linger_pending)
{
    bool intr_state = disable_interrupts();

    spinlock_acquire(&evnt->lock);

    if(evnt->num_listeners == 0) {
        if(linger_pending) {
            // There isn't a listener for the event
            // but in case there might be one later
            // the event lingers around in the queue
            evnt->num_pending += 1;
        }

        spinlock_release(&evnt->lock);
        restore_interrupts(intr_state);
        return 0;
    }

    for(size_t i = 0; i < evnt->num_listeners; ++i) {
        struct listener* dest = &evnt->listeners[i];
        struct thread* thrd = dest->thrd;

        thrd->wakeup_index = dest->wakeup_index;

        sched_enqueue(thrd);
    }

    size_t num_handled = evnt->num_listeners;

    evnt->num_listeners = 0;

    spinlock_release(&evnt->lock);
    restore_interrupts(intr_state);

    return num_handled;
}

void event_reset(struct event* restrict evnt)
{
    bool intr_state = spinlock_acquire_intr(&evnt->lock);
    evnt->num_pending = 0;
    spinlock_release_intr(&evnt->lock, intr_state);
}
