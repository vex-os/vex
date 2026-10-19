#pragma once

#include <vex/time.h>
#include <vex/types.h>

void wallclock_get(struct k_timespec* restrict out);
int wallclock_set(const struct k_timespec* restrict time);
time_t wallclock_now(void);
