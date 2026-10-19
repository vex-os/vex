#include <vex/calendar.h>

#define SECONDS_PER_DAY INT64_C(86400)
#define DAYS_PER_ERA    INT64_C(146097) // 400 Gregorian years
#define EPOCH_SHIFT     INT64_C(719468) // Days from 0000-03-01 to 1970-01-01

// Howard Hinnant's civil_from_days and days_from_civil: years are counted
// from March, so the leap day falls at the end of the year, and 400-year
// eras repeat exactly; https://howardhinnant.github.io/date_algorithms.html

static int64_t floor_div(int64_t value, int64_t divisor)
{
    int64_t quotient = value / divisor;

    if(value % divisor < 0) {
        quotient -= 1;
    }

    return quotient;
}

void calendar_from_time(time_t time, struct calendar_time* restrict out)
{
    int64_t days = floor_div(time, SECONDS_PER_DAY);
    int64_t seconds = time - days * SECONDS_PER_DAY;

    out->hour = (unsigned)(seconds / 3600);
    out->minute = (unsigned)(seconds / 60 % 60);
    out->second = (unsigned)(seconds % 60);

    int64_t shifted = days + EPOCH_SHIFT;
    int64_t era = floor_div(shifted, DAYS_PER_ERA);
    int64_t day_of_era = shifted - era * DAYS_PER_ERA;
    int64_t year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int64_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    int64_t month_from_march = (5 * day_of_year + 2) / 153;

    out->day = (unsigned)(day_of_year - (153 * month_from_march + 2) / 5 + 1);
    out->year = (long long)(year_of_era + era * 400);

    if(month_from_march < 10) {
        out->month = (unsigned)(month_from_march + 3);
    }
    else {
        out->month = (unsigned)(month_from_march - 9);
        out->year += 1;
    }
}

time_t calendar_to_time(const struct calendar_time* restrict calendar)
{
    int64_t year = (int64_t)(calendar->year);
    int64_t month_from_march = (int64_t)(calendar->month) + 9;

    if(calendar->month > 2) {
        month_from_march = (int64_t)(calendar->month) - 3;
    }
    else {
        year -= 1;
    }

    int64_t era = floor_div(year, 400);
    int64_t year_of_era = year - era * 400;
    int64_t day_of_year = (153 * month_from_march + 2) / 5 + (int64_t)(calendar->day) - 1;
    int64_t day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    int64_t days = era * DAYS_PER_ERA + day_of_era - EPOCH_SHIFT;

    return days * SECONDS_PER_DAY + calendar->hour * 3600 + calendar->minute * 60 + calendar->second;
}
