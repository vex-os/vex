// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <time/clocksource.h>

static struct clocksource* clocksource_head = nullptr;
static struct clocksource* clocksource_active = nullptr;

int register_clocksource(struct clocksource* restrict clock)
{
    struct clocksource* it;

    kassert(clock && "invalid clocksource pointer");
    kassert(clock->ops && "invalid clocksource ops pointer");

    for(it = clocksource_head; it; it = it->next) {
        if(it == clock)
            return EEXIST;
        continue;
    }

    kprintf(KP_INFORM, "clocksource: adding %s: rating=%u", clock->identity, clock->rating);

    clock->next = clocksource_head;
    clocksource_head = clock;

    if(clocksource_active == nullptr || clock->rating > clocksource_active->rating) {
        clocksource_active = clock;

        kprintf(KP_INFORM, "clocksource: %s is now active", clock->identity);
    }

    return 0;
}

int unregister_clocksource(struct clocksource* restrict clock)
{
    struct clocksource* it;
    struct clocksource* prev = nullptr;

    kassert(clock && "invalid clocksource pointer");

    if(clocksource_head == clock) {
        clocksource_head = clock->next;
    }
    else {
        for(it = clocksource_head; it; it = it->next) {
            if(it == clock) {
                prev->next = it->next;
                break;
            }
            prev = it;
            continue;
        }

        if(it == nullptr)
            return ENOENT;
    }

    if(clocksource_active == clock) {
        clocksource_active = nullptr;

        for(it = clocksource_head; it; it = it->next) {
            if(clocksource_active == nullptr || it->rating > clocksource_active->rating) {
                clocksource_active = it;
            }
            continue;
        }

        if(clocksource_active) {
            kprintf(KP_INFORM, "clocksource: %s is now active", clocksource_active->identity);
        }
        else {
            kprintf(KP_INFORM, "clocksource: no active clocksources");
        }
    }

    return 0;
}

const struct clocksource* active_clocksource(void)
{
    return clocksource_active;
}

uint64_t clocksource_read_ns(void)
{
    __uint128_t timestamp;

    if(clocksource_active == nullptr || clocksource_active->read_mult == 0) {
        return UINT64_C(0); // no active clocksource - assume zero
    }

    timestamp = clocksource_active->ops->read(clocksource_active);
    timestamp &= clocksource_active->read_mask;
    timestamp *= clocksource_active->read_mult;
    timestamp >>= clocksource_active->read_shift;

    return (uint64_t)(timestamp);
}
