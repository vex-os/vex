#pragma once

#include <vex/types.h>

// Proleptic Gregorian calendar
struct calendar_time {
    long long year;
    unsigned month; // 1 to 12
    unsigned day;   // 1 to 31
    unsigned hour;
    unsigned minute;
    unsigned second;
};

void calendar_from_time(time_t time, struct calendar_time* restrict out);
time_t calendar_to_time(const struct calendar_time* restrict calendar);
