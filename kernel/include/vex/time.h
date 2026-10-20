#pragma once

#include <usr/vex/time.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define CLOCK_REALTIME  __k_CLOCK_REALTIME
#define CLOCK_MONOTONIC __k_CLOCK_MONOTONIC

#define NSEC_PER_SEC INT64_C(1000000000)

typedef struct __k_timespec timespec_t;

static __always_inline void timespec_from_nsec(timespec_t* restrict out, uint64_t nsec)
{
    out->tv_sec = (int64_t)(nsec / NSEC_PER_SEC);
    out->tv_nsec = (int64_t)(nsec % NSEC_PER_SEC);
}
