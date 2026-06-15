#include <vex.amd64/lapic.h>
#include <vex.arch/ireq.h>
#include <vex/assert.h>
#include <vex/clockevt.h>
#include <vex/clocksrc.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/limits.h>
#include <vex/panic.h>

#define CALIBRATE_NS UINT64_C(10000000)

void clockevt_oneshot(unsigned ireq, uint64_t nsec)
{
    kassert(nsec >= 1);

    struct hart_local* hart = this_hart();

    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_ONESHOT | ((ireq + ARCH_IREQ_BASE) & UINT32_C(0xFF)));
    lapic_write(LAPIC_REG_TMRINITCNT, nsec * hart->arch.lapic_resolution);
}

void clockevt_periodic(unsigned ireq, uint64_t nsec)
{
    kassert(nsec >= 1);

    struct hart_local* hart = this_hart();

    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | ((ireq + ARCH_IREQ_BASE) & UINT32_C(0xFF)));
    lapic_write(LAPIC_REG_TMRINITCNT, nsec * hart->arch.lapic_resolution);
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

    struct hart_local* hart = this_hart();
    hart->arch.lapic_resolution = UINT32_C(0xFFFFFFFF);
    hart->arch.lapic_resolution -= lapic_read(LAPIC_REG_TMRCURRCNT);
    hart->arch.lapic_resolution /= CALIBRATE_NS;

    if(hart->arch.lapic_resolution == 0) {
        panic("clockevt: LAPIC timer calibration failed");
    }

    clockevt_stop();
}

initcall_smp_timer(arch_init_clockevt, INIT_EARLY);
