#include <vex/errno.h>
#include <vex/event.h>
#include <vex/hart.h>
#include <vex/panic.h>
#include <vex/sched.h>

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

ssize_t event_await_blocking(struct event** restrict events, size_t count)
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

    if(count > MAX_EVENTS) {
        panic("too many events");
    }

    for(size_t i = 0; i < count; ++i) {
        struct event* evnt = events[i];

        if(evnt->num_listeners >= MAX_LISTENERS) {
            panic("listener slots exhausted");
        }

        struct listener* dest = &evnt->listeners[evnt->num_listeners++];
        dest->event_index = i;
        dest->thrd = thrd;

        thrd->events[i] = evnt;
    }

    thrd->num_events = count;

    sched_dequeue(thrd);

    for(size_t i = 0; i < count; ++i) {
        spinlock_release(&events[i]->lock);
    }

    sched_yield();

    restore_interrupts(intr_state);

    for(size_t i = 0; i < thrd->num_events; ++i) {
        struct event* evnt = thrd->events[i];

        spinlock_acquire(&evnt->lock);

        for(size_t j = 0; j < evnt->num_listeners; ++j) {
            if(evnt->listeners[j].thrd == thrd) {
                evnt->num_listeners -= 1;
                evnt->listeners[j] = evnt->listeners[evnt->num_listeners];
                break;
            }
        }

        spinlock_release(&evnt->lock);
    }

    thrd->num_events = 0;

    if(thrd->enqueued_by_signal) {
        return -EINTR;
    }

    return thrd->event_index;
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

        thrd->event_index = dest->event_index;

        sched_enqueue(thrd);
    }

    size_t num_handled = evnt->num_listeners;

    evnt->num_listeners = 0;

    spinlock_release(&evnt->lock);
    restore_interrupts(intr_state);

    return num_handled;
}
