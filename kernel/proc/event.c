#include <vex/atomic.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/hart.h>
#include <vex/kmalloc.h>
#include <vex/panic.h>
#include <vex/sched.h>
#include <vex/slab.h>
#include <vex/thread.h>

static_assert(sizeof(event_t) <= SLAB_MAX_SIZE);

// A deadline of zero waits forever
static ssize_t await_until(event_t** restrict events, size_t count, uint64_t deadline, bool interruptible)
{
    ssize_t result = event_await(events, count);

    if(result >= 0) {
        return result;
    }

    thread_t* thread = this_thread();

    bool intr_state = disable_interrupts();

    for(size_t i = 0; i < count; ++i) {
        spinlock_acquire(&events[i]->lock);
    }

    // Something may have become pending since event_await dropped the locks
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

    if(count > thread->max_events) {
        event_t** grown = krealloc(thread->events, count * sizeof(event_t*));

        if(grown == nullptr) {
            for(size_t i = 0; i < count; ++i) {
                spinlock_release(&events[i]->lock);
            }

            restore_interrupts(intr_state);
            return -ENOMEM;
        }

        thread->events = grown;
        thread->max_events = count;
    }

    for(size_t i = 0; i < count; ++i) {
        event_t* event = events[i];

        if(event->num_listeners >= event->max_listeners) {
            size_t new_max_listeners = 4;

            if(event->max_listeners) {
                new_max_listeners = event->max_listeners * 2;
            }

            event_listener_t* grown = krealloc(event->listeners, new_max_listeners * sizeof(event_listener_t));

            if(grown == nullptr) {
                for(size_t j = 0; j < i; ++j) {
                    event_t* registered = events[j];

                    registered->num_listeners -= 1;

                    for(size_t k = 0; k < registered->num_listeners; ++k) {
                        if(registered->listeners[k].thread == thread) {
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

            event->listeners = grown;
            event->max_listeners = new_max_listeners;
        }

        event_listener_t* dest = &event->listeners[event->num_listeners++];
        dest->wakeup_index = i;
        dest->thread = thread;

        thread->events[i] = event;
    }

    thread->num_events = count;

    sched_dequeue(thread, interruptible);

    if(deadline) {
        sched_wake_at(thread, deadline);
    }

    for(size_t i = 0; i < count; ++i) {
        spinlock_release(&events[i]->lock);
    }

    // sched_yield waits for its own IPI, so interrupts have to be back on first
    restore_interrupts(intr_state);

    sched_yield();

    if(deadline) {
        sched_wake_cancel(thread);
    }

    // A trigger takes every listener off its event,
    // so a thread still listening everywhere timed out
    bool triggered = false;

    for(size_t i = 0; i < thread->num_events; ++i) {
        event_t* event = thread->events[i];
        bool listening = false;

        spinlock_acquire(&event->lock);

        for(size_t j = 0; j < event->num_listeners; ++j) {
            if(event->listeners[j].thread == thread) {
                event->num_listeners -= 1;
                event->listeners[j] = event->listeners[event->num_listeners];
                listening = true;
                break;
            }
        }

        spinlock_release(&event->lock);

        if(!listening) {
            triggered = true;
        }
    }

    thread->num_events = 0;

    if(!triggered) {
        if(interruptible && atomic_load(&thread->pending_work, ATOMIC_ACQUIRE)) {
            return -ERESTART;
        }

        return -ETIMEDOUT;
    }

    return thread->wakeup_index;
}

void event_init(event_t* restrict event)
{
    event->lock = SPINLOCK_INIT;
    event->num_pending = 0;
    event->num_listeners = 0;
    event->max_listeners = 0;
    event->listeners = nullptr;
}

void event_deinit(event_t* restrict event)
{
    kfree(event->listeners);

    event->num_pending = 0;
    event->num_listeners = 0;
    event->max_listeners = 0;
    event->listeners = nullptr;
}

ssize_t event_await(event_t** restrict events, size_t count)
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

ssize_t event_await_blocking(event_t** restrict events, size_t count)
{
    return await_until(events, count, 0, false);
}

ssize_t event_await_timeout(event_t** restrict events, size_t count, uint64_t timeout_nsec)
{
    uint64_t deadline = clock_source_now_ns() + timeout_nsec;

    if(deadline < timeout_nsec || deadline == 0) {
        deadline = UINT64_MAX;
    }

    return await_until(events, count, deadline, false);
}

ssize_t event_await_interruptible(event_t** restrict events, size_t count)
{
    return await_until(events, count, 0, true);
}

size_t event_trigger(event_t* restrict event, bool linger_pending)
{
    bool intr_state = disable_interrupts();

    spinlock_acquire(&event->lock);

    if(event->num_listeners == 0) {
        if(linger_pending) {
            event->num_pending += 1;
        }

        spinlock_release(&event->lock);
        restore_interrupts(intr_state);
        return 0;
    }

    for(size_t i = 0; i < event->num_listeners; ++i) {
        event_listener_t* dest = &event->listeners[i];
        thread_t* thread = dest->thread;

        thread->wakeup_index = dest->wakeup_index;

        sched_enqueue(thread);
    }

    size_t num_handled = event->num_listeners;

    event->num_listeners = 0;

    spinlock_release(&event->lock);
    restore_interrupts(intr_state);

    return num_handled;
}

void event_reset(event_t* restrict event)
{
    bool intr_state = spinlock_acquire_intr(&event->lock);
    event->num_pending = 0;
    spinlock_release_intr(&event->lock, intr_state);
}
