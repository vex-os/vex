#include <limine.h>
#include <vex/calendar.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/rtc.h>
#include <vex/seqlock.h>
#include <vex/string.h>
#include <vex/wallclock.h>

#define UPTIME_SEC_MAX    ((int64_t)(UINT64_MAX / NSEC_PER_SEC) + 1)
#define WALLCLOCK_SEC_MAX ((int64_t)(INT64_MAX - UPTIME_SEC_MAX))

#define RTC_DRIFT_SEC_MAX 2
#define RTC_ZONE_STEP_SEC 900 // Time zones are whole quarter hours apart

static volatile __used __section(".limine_requests") struct limine_date_at_boot_request date_request = {
    .id = LIMINE_DATE_AT_BOOT_REQUEST_ID,
    .revision = 0,
};

static struct k_timespec wallclock_offset = { 0 };
static struct seqlock wallclock_seqlock = SEQLOCK_INIT;

static void store_offset(const struct k_timespec* restrict offset)
{
    bool intr_state = seqlock_write_begin(&wallclock_seqlock);
    wallclock_offset = *offset;
    seqlock_write_end(&wallclock_seqlock, intr_state);
}

static void step_to(const struct k_timespec* restrict time)
{
    struct k_timespec monotonic;
    timespec_from_nsec(&monotonic, clocksrc_nsec());

    struct k_timespec offset = {
        .tv_sec = time->tv_sec - monotonic.tv_sec,
        .tv_nsec = time->tv_nsec - monotonic.tv_nsec,
    };

    if(offset.tv_nsec < 0) {
        offset.tv_nsec += NSEC_PER_SEC;
        offset.tv_sec -= 1;
    }

    store_offset(&offset);
}

static void inform_date(const char* restrict source, time_t seconds)
{
    struct calendar_time calendar;
    calendar_from_time(seconds, &calendar);

    pr_inform("wallclock: %s %04lld-%02u-%02u %02u:%02u:%02u UTC", source, calendar.year, calendar.month, calendar.day, calendar.hour,
        calendar.minute, calendar.second);
}

static bool seconds_valid(int64_t seconds)
{
    return seconds >= 0 && seconds <= WALLCLOCK_SEC_MAX;
}

static void check_rtc_drift(time_t rtc_seconds)
{
    struct k_timespec now;
    wallclock_get(&now);

    long long drift = (long long)(rtc_seconds - now.tv_sec);
    long long distance = drift;

    if(distance < 0) {
        distance = -distance;
    }

    if(distance <= RTC_DRIFT_SEC_MAX) {
        return;
    }

    pr_warning("wallclock: the RTC has drifted %lld seconds off the date at boot", drift);

    int64_t zone_rest = (int64_t)(distance % RTC_ZONE_STEP_SEC);

    if(zone_rest <= RTC_DRIFT_SEC_MAX || zone_rest >= RTC_ZONE_STEP_SEC - RTC_DRIFT_SEC_MAX) {
        pr_warning("wallclock: the RTC may be kept in local time instead of UTC");
    }
}

void wallclock_get(struct k_timespec* restrict out)
{
    struct k_timespec offset;
    unsigned sequence;

    do {
        sequence = seqlock_read_begin(&wallclock_seqlock);
        offset = wallclock_offset;
    } while(seqlock_read_retry(&wallclock_seqlock, sequence));

    struct k_timespec monotonic;
    timespec_from_nsec(&monotonic, clocksrc_nsec());

    out->tv_sec = offset.tv_sec + monotonic.tv_sec;
    out->tv_nsec = offset.tv_nsec + monotonic.tv_nsec;

    if(out->tv_nsec >= NSEC_PER_SEC) {
        out->tv_nsec -= NSEC_PER_SEC;
        out->tv_sec += 1;
    }
}

int wallclock_set(const struct k_timespec* restrict time)
{
    if(!seconds_valid(time->tv_sec)) {
        return -EINVAL;
    }

    if(time->tv_nsec < 0 || time->tv_nsec >= NSEC_PER_SEC) {
        return -EINVAL;
    }

    step_to(time);

    // The system clock is set by now, and failing to keep
    // the time over a reboot is no reason to undo that
    int retval = arch_rtc_write(time->tv_sec);

    if(retval < 0 && retval != -ENODEV) {
        pr_warning("wallclock: writing the RTC failed: %s", strerror(retval));
    }

    return 0;
}

time_t wallclock_now(void)
{
    struct k_timespec now;
    wallclock_get(&now);
    return now.tv_sec;
}

static void init_wallclock(void)
{
    struct limine_date_at_boot_response* response = date_request.response;
    time_t rtc_seconds;
    bool rtc_valid = 0 == arch_rtc_read(&rtc_seconds) && seconds_valid(rtc_seconds);

    if(response && seconds_valid(response->timestamp)) {
        struct k_timespec offset = { .tv_sec = response->timestamp, .tv_nsec = 0 };
        store_offset(&offset);

        inform_date("date at boot is", (time_t)(response->timestamp));

        if(rtc_valid) {
            check_rtc_drift(rtc_seconds);
        }

        return;
    }

    if(rtc_valid) {
        struct k_timespec now = { .tv_sec = rtc_seconds, .tv_nsec = 0 };
        step_to(&now);

        inform_date("no date at boot, the RTC says", rtc_seconds);

        return;
    }

    pr_warning("wallclock: no date at boot and no RTC, starting at the epoch");
}

initcall_bsp_timer(init_wallclock, INIT_LAST);
