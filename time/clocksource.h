// SPDX-License-Identifier: BSD-3-Clause
#ifndef B793DFAA_5257_48C9_A9BD_66862A0F1555
#define B793DFAA_5257_48C9_A9BD_66862A0F1555
#pragma once

#include <core/cdefs.h>
#include <stdint.h>
#include <time/timecounter.h>

struct clocksource;
struct clocksource_ops {
    uint64_t (*read_nsec)(struct clocksource* restrict clock);
};

struct clocksource {
    struct clocksource* next;
    const struct clocksource_ops* ops;
    struct timecounter tc;
    unsigned rating;
    char identifier[64];
    void* privdata;
};

extern int register_clocksource(struct clocksource* restrict clock);
extern int unregister_clocksource(struct clocksource* restrict clock);
extern struct clocksource* active_clocksource(void) __nodiscard;
extern uint64_t clocksource_read_nsec(void) __nodiscard;

#endif /* B793DFAA_5257_48C9_A9BD_66862A0F1555 */
