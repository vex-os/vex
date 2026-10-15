#include <vex/assert.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/kprintf.h>
#include <vex/spinlock.h>

#define NSEC_PER_SEC UINT64_C(1000000000)

static struct clocksrc* clocksrc_head = nullptr;
static struct clocksrc* clocksrc_active = nullptr;
static spinlock_type clocksrc_lock = SPINLOCK_INIT;

static void compute_mult_shift(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    uint64_t freq = scale * frequency;

    src->priv.shift = __builtin_clzll(NSEC_PER_SEC);
    src->priv.mult = (NSEC_PER_SEC << src->priv.shift) / freq;
}

static uint64_t update_nolock(void)
{
    if(clocksrc_active == nullptr) {
        return UINT64_C(0);
    }

    uint64_t now = clocksrc_active->ops->read(clocksrc_active) & clocksrc_active->mask;
    uint128_t delta;

    if(now >= clocksrc_active->priv.last) {
        delta = now - clocksrc_active->priv.last;
    }
    else {
        delta = clocksrc_active->mask - clocksrc_active->priv.last + now + UINT64_C(1);
    }

    delta *= clocksrc_active->priv.mult;
    delta >>= clocksrc_active->priv.shift;

    clocksrc_active->priv.base += delta;
    clocksrc_active->priv.last = now;

    return clocksrc_active->priv.base;
}

// The new source picks up where the
// old one left off, so time never goes backwards
static void activate_nolock(struct clocksrc* restrict src)
{
    uint64_t now_nsec = update_nolock();

    clocksrc_active = src;

    if(src) {
        src->priv.last = src->ops->read(src) & src->mask;
        src->priv.base = now_nsec;
    }
}

int add_clocksrc_raw(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    for(struct clocksrc* it = clocksrc_head; it; it = it->priv.next) {
        if(it == src) {
            return -EEXIST;
        }
    }

    pr_verbose("clocksrc: adding %s: rating=%u", src->identifier, src->rating);

    bool intr_state = spinlock_acquire_intr(&clocksrc_lock);

    compute_mult_shift(src, scale, frequency);

    src->priv.next = clocksrc_head;
    clocksrc_head = src;

    bool activated = clocksrc_active == nullptr || src->rating > clocksrc_active->rating;

    if(activated) {
        activate_nolock(src);
    }

    spinlock_release_intr(&clocksrc_lock, intr_state);

    if(activated) {
        pr_verbose("clocksrc: %s is now active", src->identifier);
    }

    return 0;
}

int calibrate_clocksrc_raw(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    for(struct clocksrc* it = clocksrc_head; it; it = it->priv.next) {
        if(it == src) {
            bool intr_state = spinlock_acquire_intr(&clocksrc_lock);

            // Ticks counted so far
            // still convert at the old rate
            if(src == clocksrc_active) {
                update_nolock();
            }

            compute_mult_shift(src, scale, frequency);

            spinlock_release_intr(&clocksrc_lock, intr_state);

            return 0;
        }
    }

    return -ENOENT;
}

int remove_clocksrc(struct clocksrc* restrict src)
{
    if(clocksrc_head == src) {
        clocksrc_head = src->priv.next;
    }
    else {
        struct clocksrc* it;
        struct clocksrc* prev = nullptr;

        for(it = clocksrc_head; it; it = it->priv.next) {
            if(it == src) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return -ENOENT;
        }
    }

    if(clocksrc_active == src) {
        struct clocksrc* best = nullptr;

        for(struct clocksrc* it = clocksrc_head; it; it = it->priv.next) {
            if(best == nullptr || it->rating > best->rating) {
                best = it;
            }
        }

        bool intr_state = spinlock_acquire_intr(&clocksrc_lock);
        activate_nolock(best);
        spinlock_release_intr(&clocksrc_lock, intr_state);

        if(best) {
            pr_verbose("clocksrc: %s is now active", best->identifier);
        }
    }

    return 0;
}

struct clocksrc* active_clocksrc(void)
{
    return clocksrc_active;
}

uint64_t clocksrc_nsec(void)
{
    bool intr_state = spinlock_acquire_intr(&clocksrc_lock);
    uint64_t now_nsec = update_nolock();
    spinlock_release_intr(&clocksrc_lock, intr_state);
    return now_nsec;
}

void clocksrc_update(void)
{
    bool intr_state = spinlock_acquire_intr(&clocksrc_lock);
    update_nolock();
    spinlock_release_intr(&clocksrc_lock, intr_state);
}
