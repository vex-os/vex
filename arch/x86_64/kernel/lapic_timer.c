#include <vex.arch/ireq.h>
#include <vex.x86/lapic.h>
#include <vex/clocksrc.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/limits.h>
#include <vex/panic.h>

#define CALIB_NS UINT64_C(10000000)
// #define CALIB_NS UINT64_C(500000000)

#if 0
static void lapic_timer_handler(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    // TODO: sched_preempt();
}
#endif

static void arch_init_lapic_timer(void)
{
#if 0
    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED | LAPIC_TIMER_ONESHOT);

    uint64_t start_ns = clocksrc_nsec();
    lapic_write(LAPIC_REG_TMRINITCNT, UINT32_C(0xFFFFFFFF));

    while(clocksrc_nsec() - start_ns < CALIB_NS) {
        // empty
    }

    uint32_t ticks_elapsed = UINT32_C(0xFFFFFFFF) - lapic_read(LAPIC_REG_TMRCURRCNT);

    struct hart_local* hart = this_hart();
    hart->x86_lapic_reload = ticks_elapsed / 10;

    if(hart->x86_lapic_reload == 0) {
        panic("lapic_timer: failed to calibrate");
    }

    unsigned irq = add_ireq_handler(UINT_MAX, &lapic_timer_handler, nullptr);

    if(irq == UINT_MAX) {
        panic("lapic_timer: out of IRQ numbers");
    }

    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | (irq + ARCH_IREQ_BASE));
    lapic_write(LAPIC_REG_TMRINITCNT, hart->x86_lapic_reload);
#endif
}

initcall_smp_timer(arch_init_lapic_timer, INIT_EARLY);
