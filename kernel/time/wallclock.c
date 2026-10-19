#include <limine.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/seqlock.h>
#include <vex/wallclock.h>

#define UPTIME_SEC_MAX    ((int64_t)(UINT64_MAX / NSEC_PER_SEC) + 1)
#define WALLCLOCK_SEC_MAX ((int64_t)(INT64_MAX - UPTIME_SEC_MAX))

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
    if(time->tv_sec < 0 || time->tv_sec > WALLCLOCK_SEC_MAX) {
        return -EINVAL;
    }

    if(time->tv_nsec < 0 || time->tv_nsec >= NSEC_PER_SEC) {
        return -EINVAL;
    }

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

    if(response == nullptr || response->timestamp < 0 || response->timestamp > WALLCLOCK_SEC_MAX) {
        pr_warning("wallclock: no valid date at boot, starting at the epoch");
        return;
    }

    struct k_timespec offset = { .tv_sec = response->timestamp, .tv_nsec = 0 };
    store_offset(&offset);

    pr_inform("wallclock: date at boot is %ld", response->timestamp);
}

initcall_bsp_bootinfo(init_wallclock, INIT_MIDDLE);
