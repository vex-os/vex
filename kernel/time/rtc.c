#include <vex/errno.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/rtc.h>
#include <vex/wallclock.h>

static mutex_t rtc_mutex = MUTEX_INIT; // Held while an op runs, so removing a clock waits for it
static rtc_t* rtc_head = nullptr;

int rtc_add(rtc_t* restrict rtc)
{
    mutex_acquire(&rtc_mutex);

    rtc_t** tail = &rtc_head;

    while(*tail) {
        if(*tail == rtc) {
            mutex_release(&rtc_mutex);
            return -EEXIST;
        }

        tail = &(*tail)->priv.next;
    }

    rtc->priv.next = nullptr;
    *tail = rtc;

    bool in_use = rtc_head == rtc;

    mutex_release(&rtc_mutex);

    pr_verbose("rtc: added %s", rtc->name);

    if(in_use) {
        wallclock_rtc_arrived();
    }

    return 0;
}

void rtc_remove(rtc_t* restrict rtc)
{
    mutex_acquire(&rtc_mutex);

    rtc_t** prev = &rtc_head;

    while(*prev && *prev != rtc) {
        prev = &(*prev)->priv.next;
    }

    if(*prev) {
        *prev = rtc->priv.next;
        rtc->priv.next = nullptr;
    }

    mutex_release(&rtc_mutex);

    pr_verbose("rtc: removed %s", rtc->name);
}

int rtc_read(time_t* restrict time)
{
    mutex_acquire(&rtc_mutex);

    int retval = -ENODEV;

    if(rtc_head) {
        retval = rtc_head->ops->read(rtc_head, time);
    }

    mutex_release(&rtc_mutex);

    return retval;
}

int rtc_write(time_t time)
{
    mutex_acquire(&rtc_mutex);

    int retval = -ENODEV;

    if(rtc_head) {
        retval = rtc_head->ops->write(rtc_head, time);
    }

    mutex_release(&rtc_mutex);

    return retval;
}
