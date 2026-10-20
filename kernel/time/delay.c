#include <vex/clocksrc.h>
#include <vex/delay.h>
#include <vex/hart.h>

void delay_msec(uint64_t msec)
{
    uint64_t deadline = msec * 1000000 + clock_source_now_ns();

    while(clock_source_now_ns() < deadline) {
        hart_spin();
    }
}

void delay_usec(uint64_t usec)
{
    uint64_t deadline = usec * 1000 + clock_source_now_ns();

    while(clock_source_now_ns() < deadline) {
        hart_spin();
    }
}

void delay_nsec(uint64_t nsec)
{
    uint64_t deadline = nsec + clock_source_now_ns();

    while(clock_source_now_ns() < deadline) {
        hart_spin();
    }
}
