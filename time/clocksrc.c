#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/types.h>
#include <time/clocksrc.h>

#define FEMTO_PER_NANO UINT64_C(1000000)

static struct clocksrc* clocksrc_head = nullptr;
static struct clocksrc* clocksrc_active = nullptr;

int register_clocksrc(struct clocksrc* restrict src, uint64_t period_fsec, uint64_t initial_cycle)
{
    struct clocksrc* it;

    for(it = clocksrc_head; it; it = it->next) {
        if(it == src)
            return EEXIST;
        continue;
    }

    pr_debug("clocksrc: adding %s: rating=%u", src->identifier, src->rating);

    src->calc_shift = 32;
    src->calc_mult = (period_fsec << 32) / FEMTO_PER_NANO;
    src->last_cycle = initial_cycle & src->calc_mask;
    src->base_nsec = 0;

    src->next = clocksrc_head;
    clocksrc_head = src;

    if(clocksrc_active == nullptr || src->rating > clocksrc_active->rating) {
        clocksrc_active = src;

        pr_debug("clocksrc: %s is now active", src->identifier);
    }

    return 0;
}

int unregister_clocksrc(struct clocksrc* restrict src)
{
    struct clocksrc* it;
    struct clocksrc* prev = nullptr;

    if(clocksrc_head == src) {
        clocksrc_head = src->next;
    }
    else {
        for(it = clocksrc_head; it; it = it->next) {
            if(it == src) {
                prev->next = it->next;
                break;
            }
            prev = it;
            continue;
        }

        if(it == nullptr)
            return ENOENT;
    }

    if(clocksrc_active == src) {
        clocksrc_active = nullptr;

        for(it = clocksrc_head; it; it = it->next) {
            if(clocksrc_active == nullptr || it->rating > clocksrc_active->rating) {
                clocksrc_active = it;
            }
            continue;
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
    uint64_t now;
    uint128_t delta;

    if(clocksrc_active == nullptr) {
        return UINT64_C(0);
    }

    now = clocksrc_active->ops->read(clocksrc_active);
    now &= clocksrc_active->calc_mask;

    if(now >= clocksrc_active->last_cycle) {
        delta = now - clocksrc_active->last_cycle;
    }
    else {
        delta = clocksrc_active->calc_mask - clocksrc_active->last_cycle + now + UINT64_C(1);
    }

    delta *= clocksrc_active->calc_mult;
    delta >>= clocksrc_active->calc_shift;

    clocksrc_active->base_nsec += delta;
    clocksrc_active->last_cycle = now;

    return clocksrc_active->base_nsec;
}
