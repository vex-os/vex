#ifndef CBD0B069_8008_451E_9CC2_D263E53CB955
#define CBD0B069_8008_451E_9CC2_D263E53CB955
#pragma once

#include <vex/types.h>

void clockevt_oneshot(unsigned ireq, uint64_t msec);
void clockevt_periodic(unsigned ireq, uint64_t msec);
void clockevt_stop(void);

#endif /* CBD0B069_8008_451E_9CC2_D263E53CB955 */
