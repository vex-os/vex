// SPDX-License-Identifier: BSD-3-Clause
#ifndef EA9AF93E_1144_4DBA_A3CB_CDF4C67F7951
#define EA9AF93E_1144_4DBA_A3CB_CDF4C67F7951
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

/// Monotonic nanosecond accumulator for hardware cycle counters;
/// This handles counter overflow (eg. the one that happens in
/// the ACPI PM timer); converts cycles to nanoseconds via mult/shift;
/// The formula is the following: nsec = (cycles * mult) >> shift
struct timecounter {
    uint64_t last_cycle;
    uint64_t base_nsec;
    uint64_t calc_mask;
    uint64_t calc_mult;
    uint32_t calc_shift;
};

extern void timecounter_init(struct timecounter* restrict tc, uint64_t period_fsec, uint64_t initial_cycle);
extern uint64_t timecounter_update(struct timecounter* restrict tc, uint64_t now) __nodiscard;

#endif /* EA9AF93E_1144_4DBA_A3CB_CDF4C67F7951 */
