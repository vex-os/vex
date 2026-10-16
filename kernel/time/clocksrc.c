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

static bool is_registered_nolock(const struct clocksrc* restrict src)
{
    for(struct clocksrc* it = clocksrc_head; it; it = it->priv.next) {
        if(it == src) {
            return true;
        }
    }

    return false;
}

int add_clocksrc_raw(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    bool intr_state = spinlock_acquire_intr(&clocksrc_lock);

    if(is_registered_nolock(src)) {
        spinlock_release_intr(&clocksrc_lock, intr_state);
        return -EEXIST;
    }

    compute_mult_shift(src, scale, frequency);

    src->priv.next = clocksrc_head;
    clocksrc_head = src;

    bool activated = clocksrc_active == nullptr || src->rating > clocksrc_active->rating;

    if(activated) {
        activate_nolock(src);
    }

    spinlock_release_intr(&clocksrc_lock, intr_state);

    pr_verbose("clocksrc: added %s: rating=%u", src->identifier, src->rating);

    if(activated) {
        pr_verbose("clocksrc: %s is now active", src->identifier);
    }

    return 0;
}

int calibrate_clocksrc_raw(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    bool intr_state = spinlock_acquire_intr(&clocksrc_lock);

    if(!is_registered_nolock(src)) {
        spinlock_release_intr(&clocksrc_lock, intr_state);
        return -ENOENT;
    }

    // Ticks counted so far
    // still convert at the old rate
    if(src == clocksrc_active) {
        update_nolock();
    }

    compute_mult_shift(src, scale, frequency);

    spinlock_release_intr(&clocksrc_lock, intr_state);

    return 0;
}

int remove_clocksrc(struct clocksrc* restrict src)
{
    bool intr_state = spinlock_acquire_intr(&clocksrc_lock);

    struct clocksrc** prev = &clocksrc_head;

    while(*prev && *prev != src) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        spinlock_release_intr(&clocksrc_lock, intr_state);
        return -ENOENT;
    }

    *prev = src->priv.next;

    struct clocksrc* best = nullptr;
    bool was_active = clocksrc_active == src;

    if(was_active) {
        for(struct clocksrc* it = clocksrc_head; it; it = it->priv.next) {
            if(best == nullptr || it->rating > best->rating) {
                best = it;
            }
        }

        activate_nolock(best);
    }

    spinlock_release_intr(&clocksrc_lock, intr_state);

    if(was_active && best) {
        pr_verbose("clocksrc: %s is now active", best->identifier);
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
