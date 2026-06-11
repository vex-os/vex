#include <vex/clocksrc.h>
#include <vex/delay.h>
#include <vex/hart.h>

void delay_msec(uint64_t msec)
{
    uint64_t deadline = msec * 1000000 + clocksrc_nsec();

    while(clocksrc_nsec() < deadline) {
        hart_spin();
    }
}

void delay_usec(uint64_t usec)
{
    uint64_t deadline = usec * 1000 + clocksrc_nsec();

    while(clocksrc_nsec() < deadline) {
        hart_spin();
    }
}

void delay_nsec(uint64_t nsec)
{
    uint64_t deadline = nsec + clocksrc_nsec();

    while(clocksrc_nsec() < deadline) {
        hart_spin();
    }
}
