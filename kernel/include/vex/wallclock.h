#pragma once

#include <vex/time.h>
#include <vex/types.h>

void wallclock_get(timespec_t* restrict out);
int wallclock_set(const timespec_t* restrict time);
time_t wallclock_now(void);
