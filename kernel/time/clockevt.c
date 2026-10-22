#include <vex/clockevt.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/kprintf.h>
#include <vex/spinlock.h>

typedef struct hart_timer_s hart_timer_t;

struct hart_timer_s {
    spinlock_t lock; // Raw, with interrupts disabled, as the scheduler's interrupt handler takes it
    clock_event_t* active;
    unsigned irq; // What the timer was last set to raise
    bool armed;
};

static spinlock_t registry_lock = SPINLOCK_INIT; // Taken with interrupts disabled, before any hart's lock
static clock_event_t* event_head = nullptr;
static hart_timer_t hart_timers[MAX_HARTS];

// Device interrupts go to the boot hart alone, so a global timer serves it alone;
// UNDONE: broadcast from a global timer to the harts whose own stops in idle
static unsigned hart_index_of(const clock_event_t* restrict event)
{
    if(event->features & CLOCK_EVENT_PER_HART) {
        hart_local_t* hart = this_hart();
        return hart->index;
    }

    hart_local_t* hart = boot_hart();

    return hart->index;
}

static clock_event_t* best_for_nolock(unsigned hart_index, const clock_event_t* restrict except)
{
    clock_event_t* best = nullptr;

    for(clock_event_t* it = event_head; it; it = it->priv.next) {
        if(it == except || it->priv.hart_index != hart_index) {
            continue;
        }

        if(best == nullptr || it->rating > best->rating) {
            best = it;
        }
    }

    return best;
}

static void program(clock_event_t* restrict event, unsigned irq, uint64_t nsec, bool periodic)
{
    if(nsec < event->min_delta_ns) {
        nsec = event->min_delta_ns;
    }

    if(nsec > event->max_delta_ns) {
        nsec = event->max_delta_ns;
    }

    bool use_periodic = periodic && (event->features & CLOCK_EVENT_PERIODIC);

    if(use_periodic || 0 == (event->features & CLOCK_EVENT_ONESHOT)) {
        event->ops->set_periodic(event, irq, nsec);
    }
    else {
        event->ops->set_oneshot(event, irq, nsec);
    }
}

// An armed hart has to hear from its new timer: this one is set to fire soon,
// another is sent the interrupt, and either way the handler sets the timer again
static void switch_nolock(hart_timer_t* restrict timer, unsigned hart_index, clock_event_t* restrict next, bool* restrict send_ipi)
{
    clock_event_t* prev = timer->active;
    hart_local_t* hart = this_hart();
    bool here = hart->index == hart_index;

    *send_ipi = false;

    if(prev == next) {
        return;
    }

    if(prev && (here || 0 == (prev->features & CLOCK_EVENT_PER_HART))) {
        prev->ops->stop(prev);
    }

    timer->active = next;

    if(!timer->armed) {
        return;
    }

    if(here) {
        program(next, timer->irq, next->min_delta_ns, false);
    }
    else {
        *send_ipi = true;
    }
}

static void announce(const clock_event_t* restrict event, unsigned hart_index, bool send_ipi, unsigned irq)
{
    pr_verbose("clockevt: %s is now active on hart %u", event->name, hart_index);

    if(send_ipi) {
        hart_ipi(&harts[hart_index], irq);
    }
}

int clock_event_add(clock_event_t* restrict event)
{
    bool can_fire = event->features & (CLOCK_EVENT_ONESHOT | CLOCK_EVENT_PERIODIC);

    if(!can_fire || event->min_delta_ns > event->max_delta_ns) {
        return -EINVAL;
    }

    bool intr_state = spinlock_acquire_intr(&registry_lock);

    for(clock_event_t* it = event_head; it; it = it->priv.next) {
        if(it == event) {
            spinlock_release_intr(&registry_lock, intr_state);
            return -EEXIST;
        }
    }

    unsigned hart_index = hart_index_of(event);

    event->priv.hart_index = hart_index;
    event->priv.next = event_head;
    event_head = event;

    hart_timer_t* timer = &hart_timers[hart_index];
    raw_spinlock_acquire(&timer->lock);

    clock_event_t* best = best_for_nolock(hart_index, nullptr);
    bool switched = timer->active != best;
    bool send_ipi;
    switch_nolock(timer, hart_index, best, &send_ipi);
    unsigned irq = timer->irq;

    raw_spinlock_release(&timer->lock);
    spinlock_release_intr(&registry_lock, intr_state);

    size_t min_delta_ns = event->min_delta_ns;
    pr_verbose("clockevt: added %s: rating=%u, min_delta=%zu ns", event->name, event->rating, min_delta_ns);

    if(switched) {
        announce(best, hart_index, send_ipi, irq);
    }

    return 0;
}

int clock_event_remove(clock_event_t* restrict event)
{
    bool intr_state = spinlock_acquire_intr(&registry_lock);

    clock_event_t** prev = &event_head;

    while(*prev && *prev != event) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        spinlock_release_intr(&registry_lock, intr_state);
        return -ENOENT;
    }

    unsigned hart_index = event->priv.hart_index;
    hart_timer_t* timer = &hart_timers[hart_index];
    raw_spinlock_acquire(&timer->lock);

    bool was_active = timer->active == event;
    clock_event_t* next = best_for_nolock(hart_index, event);

    if(was_active && next == nullptr) {
        raw_spinlock_release(&timer->lock);
        spinlock_release_intr(&registry_lock, intr_state);
        return -EBUSY;
    }

    *prev = event->priv.next;
    event->priv.next = nullptr;

    bool send_ipi = false;

    if(was_active) {
        switch_nolock(timer, hart_index, next, &send_ipi);
    }

    unsigned irq = timer->irq;

    raw_spinlock_release(&timer->lock);
    spinlock_release_intr(&registry_lock, intr_state);

    pr_verbose("clockevt: removed %s", event->name);

    if(was_active) {
        announce(next, hart_index, send_ipi, irq);
    }

    return 0;
}

void clock_event_oneshot(unsigned irq, uint64_t nsec)
{
    hart_local_t* hart = this_hart();
    hart_timer_t* timer = &hart_timers[hart->index];

    raw_spinlock_acquire(&timer->lock);

    timer->irq = irq;
    timer->armed = true;

    if(timer->active) {
        program(timer->active, irq, nsec, false);
    }

    raw_spinlock_release(&timer->lock);
}

void clock_event_periodic(unsigned irq, uint64_t nsec)
{
    hart_local_t* hart = this_hart();
    hart_timer_t* timer = &hart_timers[hart->index];

    raw_spinlock_acquire(&timer->lock);

    timer->irq = irq;
    timer->armed = true;

    if(timer->active) {
        program(timer->active, irq, nsec, true);
    }

    raw_spinlock_release(&timer->lock);
}

void clock_event_stop(void)
{
    hart_local_t* hart = this_hart();
    hart_timer_t* timer = &hart_timers[hart->index];

    raw_spinlock_acquire(&timer->lock);

    if(timer->active) {
        clock_event_t* event = timer->active;
        event->ops->stop(event);
    }

    timer->armed = false;

    raw_spinlock_release(&timer->lock);
}
