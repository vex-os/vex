#pragma once

#include <vex/types.h>

void clock_event_oneshot(unsigned irq, uint64_t nsec);
void clock_event_periodic(unsigned irq, uint64_t nsec);
void clock_event_stop(void);
