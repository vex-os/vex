#pragma once

#include <vex/types.h>

typedef struct calendar_time_s calendar_time_t;

// Proleptic Gregorian calendar
struct calendar_time_s {
    long long year;
    unsigned month; // 1 to 12
    unsigned day;   // 1 to 31
    unsigned hour;
    unsigned minute;
    unsigned second;
};

void calendar_from_time(time_t time, calendar_time_t* restrict out);
time_t calendar_to_time(const calendar_time_t* restrict calendar);
