// SPDX-License-Identifier: BSD-3-Clause
#ifndef B793DFAA_5257_48C9_A9BD_66862A0F1555
#define B793DFAA_5257_48C9_A9BD_66862A0F1555
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

struct clocksource;
struct clocksource_ops {
    uint64_t (*read)(const struct clocksource* restrict clock);
};

struct clocksource {
    struct clocksource* next;
    const struct clocksource_ops* ops;
    char identity[64];
    unsigned int rating;
    uint64_t read_mask;
    uint64_t read_shift;
    uint64_t read_mult;
};

extern int register_clocksource(struct clocksource* restrict clock);
extern int unregister_clocksource(struct clocksource* restrict clock);
extern const struct clocksource* active_clocksource(void) __nodiscard;
extern uint64_t clocksource_read_ns(void) __nodiscard;

#endif /* B793DFAA_5257_48C9_A9BD_66862A0F1555 */
