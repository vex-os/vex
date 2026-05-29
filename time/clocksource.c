#include <core/assert.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <time/clocksource.h>

static struct clocksource* clocksource_head = nullptr;
static struct clocksource* clocksource_active = nullptr;

int register_clocksource(struct clocksource* restrict clock)
{
    struct clocksource* it;

    for(it = clocksource_head; it; it = it->next) {
        if(it == clock)
            return EEXIST;
        continue;
    }

    pr_inform("clocksource: adding %s: rating=%u", clock->identifier, clock->rating);

    clock->next = clocksource_head;
    clocksource_head = clock;

    if(clocksource_active == nullptr || clock->rating > clocksource_active->rating) {
        clocksource_active = clock;

        pr_inform("clocksource: %s is now active", clock->identifier);
    }

    return 0;
}

int unregister_clocksource(struct clocksource* restrict clock)
{
    struct clocksource* it;
    struct clocksource* prev = nullptr;

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
            pr_inform("clocksource: %s is now active", clocksource_active->identifier);
        }
        else {
            pr_inform("clocksource: no active clocksources");
        }
    }

    return 0;
}

struct clocksource* active_clocksource(void)
{
    return clocksource_active;
}

uint64_t clocksource_read_nsec(void)
{
    if(clocksource_active == nullptr) {
        return UINT64_C(0);
    }

    return clocksource_active->ops->read_nsec(clocksource_active);
}
