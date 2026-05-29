#include <time/timecounter.h>

#define FEMTO_PER_NANO UINT64_C(1000000)

void timecounter_init(struct timecounter* restrict tc, uint64_t period_fsec, uint64_t initial_cycle)
{
    tc->calc_shift = 32;
    tc->calc_mult = (period_fsec << 32) / FEMTO_PER_NANO;
    tc->last_cycle = initial_cycle & tc->calc_mask;
    tc->base_nsec = 0;
}

uint64_t timecounter_update(struct timecounter* restrict tc, uint64_t now)
{
    __uint128_t delta;

    now &= tc->calc_mask;

    if(now >= tc->last_cycle) {
        delta = now - tc->last_cycle;
    }
    else {
        delta = tc->calc_mask - tc->last_cycle + now + UINT64_C(1);
    }

    delta *= tc->calc_mult;
    delta >>= tc->calc_shift;

    tc->base_nsec += delta;
    tc->last_cycle = now;

    return tc->base_nsec;
}
