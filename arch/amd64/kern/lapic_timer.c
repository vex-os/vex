#include <limits.h>
#include <sys.amd64/lapic.h>
#include <sys.arch/hart.h>
#include <sys.arch/ireq.h>
#include <sys/clocksrc.h>
#include <sys/initcall.h>
#include <sys/ireq.h>
#include <sys/panic.h>
#include <sys/sched.h>

#define CALIB_NS UINT64_C(10000000)

static void lapic_timer_handler(unsigned number, struct hart_regs* restrict regs, void* restrict ctxdata)
{
    sched_tick();
}

define_initcall(amd64_lapic_timer, INITCALL_STAGE_SMP_TIMER, INITCALL_ORDER_EARLY)
{
    lapic_write(LAPIC_REG_TMRDIV, LAPIC_TIMER_DIV1);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED | LAPIC_TIMER_ONESHOT);

    uint64_t start_ns = clocksrc_nsec();
    lapic_write(LAPIC_REG_TMRINITCNT, UINT32_C(0xFFFFFFFF));

    while(clocksrc_nsec() - start_ns < CALIB_NS) {
        // empty
    }

    uint32_t ticks_elapsed = UINT32_C(0xFFFFFFFF) - lapic_read(LAPIC_REG_TMRCURRCNT);

    struct hart_local* hart = this_hart();
    hart->amd64.lapic_rld = ticks_elapsed / 10;

    if(hart->amd64.lapic_rld == 0) {
        panic("lapic_timer: failed to calibrate");
    }

    unsigned irq = add_ireq_handler(UINT_MAX, &lapic_timer_handler, nullptr);

    if(irq == UINT_MAX) {
        panic("lapic_timer: out of IRQ numbers");
    }

    // Start periodic timer
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | (irq + ARCH_IREQ_BASE));
    lapic_write(LAPIC_REG_TMRINITCNT, hart->amd64.lapic_rld);
}
