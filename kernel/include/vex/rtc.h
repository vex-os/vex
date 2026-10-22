#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct rtc_s rtc_t;
typedef struct rtc_ops_s rtc_ops_t;
typedef struct rtc_private_s rtc_private_t;

struct rtc_ops_s {
    /// @retval -EIO The clock holds no valid time
    int (*read)(rtc_t* restrict rtc, time_t* restrict time);

    /// @retval -EINVAL The clock can't hold the time, as with a year outside its range
    int (*write)(rtc_t* restrict rtc, time_t time);
};

struct rtc_private_s {
    rtc_t* next;
};

// A clock that keeps the time while the system is off, in seconds since the epoch, UTC
struct rtc_s {
    rtc_private_t priv;
    const rtc_ops_t* ops;
    const char* name;
    void* ctxdata;
};

/// @retval -EEXIST The clock is already added
/// @note The first clock added is the one in use, until it is removed
int rtc_add(rtc_t* restrict rtc);

/// @post None of the clock's ops runs, or will
void rtc_remove(rtc_t* restrict rtc);

/// @retval -ENODEV There is no clock
/// @retval -EIO The clock holds no valid time
int rtc_read(time_t* restrict time) __nodiscard;

/// @retval -ENODEV There is no clock
/// @retval -EINVAL The clock can't hold the time
int rtc_write(time_t time);
