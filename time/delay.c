#include <arch/hart.h>
#include <time/clocksrc.h>
#include <time/delay.h>

void delay_msec(uint64_t msec)
{
    uint64_t deadline = msec * 1000000 + clocksrc_nsec();

    while(clocksrc_nsec() < deadline) {
        yield_hart();
    }
}

void delay_usec(uint64_t usec)
{
    uint64_t deadline = usec * 1000 + clocksrc_nsec();

    while(clocksrc_nsec() < deadline) {
        yield_hart();
    }
}

void delay_nsec(uint64_t nsec)
{
    uint64_t deadline = nsec + clocksrc_nsec();

    while(clocksrc_nsec() < deadline) {
        yield_hart();
    }
}
