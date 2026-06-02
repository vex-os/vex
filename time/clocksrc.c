#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/types.h>
#include <time/clocksrc.h>

#define NSEC_PER_SEC UINT64_C(1000000000)

static struct clocksrc* clocksrc_head = nullptr;
static struct clocksrc* clocksrc_active = nullptr;

static void compute_mult_shift(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    uint64_t freq = scale * frequency;

    src->priv.shift = __builtin_clzll(NSEC_PER_SEC);
    src->priv.mult = (NSEC_PER_SEC << src->priv.shift) / freq;
}

int clocksrc_register_internal(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    struct clocksrc* it;

    for(it = clocksrc_head; it; it = it->priv.next) {
        if(it == src) {
            return EEXIST;
        }
    }

    pr_debug("clocksrc: adding %s: rating=%u", src->identifier, src->rating);

    compute_mult_shift(src, scale, frequency);
    src->priv.last = src->ops->read(src) & src->mask;
    src->priv.base = 0;

    src->priv.next = clocksrc_head;
    clocksrc_head = src;

    if(clocksrc_active == nullptr || src->rating > clocksrc_active->rating) {
        clocksrc_active = src;

        pr_debug("clocksrc: %s is now active", src->identifier);
    }

    return 0;
}

int clocksrc_calibrate_internal(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency)
{
    struct clocksrc* it;

    for(it = clocksrc_head; it; it = it->priv.next) {
        if(it == src) {
            compute_mult_shift(src, scale, frequency);
            return 0;
        }
    }

    return ENOENT;
}

int clocksrc_unregister(struct clocksrc* restrict src)
{
    struct clocksrc* it;
    struct clocksrc* prev = nullptr;

    if(clocksrc_head == src) {
        clocksrc_head = src->priv.next;
    }
    else {
        for(it = clocksrc_head; it; it = it->priv.next) {
            if(it == src) {
                prev->priv.next = it->priv.next;
                break;
            }

            prev = it;
        }

        if(it == nullptr) {
            return ENOENT;
        }
    }

    if(clocksrc_active == src) {
        clocksrc_active = nullptr;

        for(it = clocksrc_head; it; it = it->priv.next) {
            if(clocksrc_active == nullptr || it->rating > clocksrc_active->rating) {
                clocksrc_active = it;
            }
        }

        if(clocksrc_active) {
            pr_debug("clocksrc: %s is now active", clocksrc_active->identifier);
        }
    }

    return 0;
}

struct clocksrc* active_clocksrc(void)
{
    return clocksrc_active;
}

uint64_t clocksrc_read_nsec(void)
{
    uint128_t delta;
    uint64_t now;

    if(clocksrc_active == nullptr) {
        return UINT64_C(0);
    }

    now = clocksrc_active->ops->read(clocksrc_active) & clocksrc_active->mask;

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
