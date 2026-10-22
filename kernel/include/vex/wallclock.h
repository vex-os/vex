#pragma once

#include <vex/time.h>
#include <vex/types.h>

void wallclock_get(timespec_t* restrict out);
int wallclock_set(const timespec_t* restrict time);
time_t wallclock_now(void);

/// Checks the time against the RTC that just came into use, or takes the time from it when nothing else gave one
/// @pre Called by rtc_add, without the RTCs' lock
void wallclock_rtc_arrived(void);
