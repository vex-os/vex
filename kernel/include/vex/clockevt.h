#pragma once

#include <vex/types.h>

void clockevt_oneshot(unsigned ireq, uint64_t nsec);
void clockevt_periodic(unsigned ireq, uint64_t nsec);
void clockevt_stop(void);
