#include <arch/amd64.lapic.h>
#include <arch/ireq.h>
#include <vex/assert.h>
#include <vex/clockevt.h>
#include <vex/clocksrc.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/limits.h>
#include <vex/panic.h>

#define CALIBRATE_NS UINT64_C(10000000)
#define NS_PER_SEC   UINT64_C(1000000000)

static uint32_t timer_count(uint64_t nsec)
{
    const struct hart_local* hart = this_hart();

    uint64_t hz = hart->arch.lapic_timer_hz;

    if(nsec > UINT64_MAX / hz) {
        return UINT32_MAX;
    }

    uint64_t count = nsec * hz / NS_PER_SEC;

    // Zero initial count disarms the
    // timer instead of firing it right away
    if(count == 0) {
        return 1;
    }

    if(count > UINT32_MAX) {
        return UINT32_MAX;
    }

    return count;
}

void clockevt_oneshot(unsigned ireq, uint64_t nsec)
{
    kassert(nsec >= 1);

    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_ONESHOT | ((ireq + ARCH_IREQ_BASE) & UINT32_C(0xFF)));
    lapic_write(LAPIC_REG_TMRINITCNT, timer_count(nsec));
}

void clockevt_periodic(unsigned ireq, uint64_t nsec)
{
    kassert(nsec >= 1);

    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | ((ireq + ARCH_IREQ_BASE) & UINT32_C(0xFF)));
    lapic_write(LAPIC_REG_TMRINITCNT, timer_count(nsec));
}

void clockevt_stop(void)
{
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TMRINITCNT, 0);
}

static void arch_init_clockevt(void)
{
    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED | LAPIC_TIMER_ONESHOT);
    lapic_write(LAPIC_REG_TMRINITCNT, UINT32_C(0xFFFFFFFF));

    uint64_t start_nsec = clocksrc_nsec();
    uint64_t now_nsec = start_nsec;

    while(now_nsec < start_nsec + CALIBRATE_NS) {
        now_nsec = clocksrc_nsec();
    }

    uint64_t ticks = UINT32_C(0xFFFFFFFF) - lapic_read(LAPIC_REG_TMRCURRCNT);

    struct hart_local* hart = this_hart();
    hart->arch.lapic_timer_hz = ticks * NS_PER_SEC / (now_nsec - start_nsec);

    if(hart->arch.lapic_timer_hz == 0) {
        panic("clockevt: LAPIC timer calibration failed");
    }

    clockevt_stop();
}

initcall_smp_timer(arch_init_clockevt, INIT_EARLY);
