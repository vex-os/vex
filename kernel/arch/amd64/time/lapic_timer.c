#include <arch/amd64.lapic.h>
#include <arch/irq.h>
#include <vex/clockevt.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/string.h>

#define CALIBRATE_NS UINT64_C(10000000)
#define NS_PER_SEC   UINT64_C(1000000000)

static clock_event_t lapic_timers[MAX_HARTS];

static uint32_t timer_count(const clock_event_t* restrict event, uint64_t nsec)
{
    const hart_local_t* hart = event->ctxdata;
    uint64_t count = nsec * hart->arch.lapic_timer_hz / NS_PER_SEC;

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

static void lapic_timer_set_oneshot(clock_event_t* restrict event, unsigned irq, uint64_t nsec)
{
    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_ONESHOT | ((irq + ARCH_IRQ_BASE) & UINT32_C(0xFF)));
    lapic_write(LAPIC_REG_TMRINITCNT, timer_count(event, nsec));
}

static void lapic_timer_set_periodic(clock_event_t* restrict event, unsigned irq, uint64_t nsec)
{
    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | ((irq + ARCH_IRQ_BASE) & UINT32_C(0xFF)));
    lapic_write(LAPIC_REG_TMRINITCNT, timer_count(event, nsec));
}

static void lapic_timer_stop(clock_event_t* restrict event)
{
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TMRINITCNT, 0);
}

static const clock_event_ops_t lapic_timer_ops = {
    .set_oneshot = &lapic_timer_set_oneshot,
    .set_periodic = &lapic_timer_set_periodic,
    .stop = &lapic_timer_stop,
};

static uint64_t calibrate(void)
{
    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED | LAPIC_TIMER_ONESHOT);
    lapic_write(LAPIC_REG_TMRINITCNT, UINT32_C(0xFFFFFFFF));

    uint64_t start_nsec = clock_source_now_ns();
    uint64_t now_nsec = start_nsec;

    while(now_nsec < start_nsec + CALIBRATE_NS) {
        now_nsec = clock_source_now_ns();
    }

    uint64_t ticks = UINT32_C(0xFFFFFFFF) - lapic_read(LAPIC_REG_TMRCURRCNT);

    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TMRINITCNT, 0);

    return ticks * NS_PER_SEC / (now_nsec - start_nsec);
}

// A hart whose timer doesn't calibrate
// goes on with a global one, if it may use any
static void arch_init_lapic_timer(void)
{
    hart_local_t* hart = this_hart();
    uint64_t hz = calibrate();

    if(hz == 0) {
        pr_warning("lapic_timer: calibration failed on hart %u", (unsigned)(hart->index));
        return;
    }

    hart->arch.lapic_timer_hz = hz;

    clock_event_t* event = &lapic_timers[hart->index];
    event->ops = &lapic_timer_ops;
    event->name = "lapic_timer";
    event->rating = 300;
    event->features = CLOCK_EVENT_ONESHOT | CLOCK_EVENT_PERIODIC | CLOCK_EVENT_PER_HART;
    event->min_delta_ns = (NS_PER_SEC + hz - 1) / hz;
    event->max_delta_ns = UINT64_C(0xFFFFFFFF) * NS_PER_SEC / hz;
    event->ctxdata = hart;

    int retval = clock_event_add(event);

    if(retval < 0) {
        pr_warning("lapic_timer: %s", strerror(retval));
    }
}

initcall_smp_timer(arch_init_lapic_timer, INIT_EARLY);
