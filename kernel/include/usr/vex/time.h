#ifndef USR_VEX_TIME_H
#define USR_VEX_TIME_H

#include <vex/types.h>

/* NOTE: __k_CLOCK numbers are same as ones
 * defined in Linux, so that libc needs no translation */

#define __k_CLOCK_REALTIME  0
#define __k_CLOCK_MONOTONIC 1

struct __k_timespec {
    __k_time_t tv_sec;
    __k_int64_t tv_nsec;
};

#endif /* USR_VEX_TIME_H */
