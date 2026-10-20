#include <vex/atomic.h>
#include <vex/console.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/irq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/panic.h>

// Somewhere around a second of pause instructions; the clock source can't time
// this, as it takes a lock that a hart which ignores the IPI may be holding
#define STOP_WAIT_SPINS 0x1000000UL

volatile bool panic_in_progress = false;

static bool panic_claimed = false;
static unsigned long stopped_harts = 0U;
static unsigned panic_ipi_number = UINT_MAX;

static void __noreturn halt_stopped(void)
{
    atomic_fetch_add(&stopped_harts, 1, ATOMIC_RELEASE);

    for(;;) {
        disable_interrupts();
        hart_spin();
    }
}

static void panic_ipi_irq(unsigned number, const hart_context_t* restrict regs, void* restrict ctxdata)
{
    halt_stopped();
}

// The panicking hart is one of them, unless it panicked before coming online;
// an AP that does so counts one hart too many and waits out the timeout
static unsigned long count_other_harts(void)
{
    unsigned long online = 0U;

    for(unsigned long i = 0U; i < num_harts; ++i) {
        unsigned short flags = atomic_load_n(&harts[i].flags, ATOMIC_ACQUIRE);

        if(flags & HART_FLAG_ONLINE) {
            online += 1U;
        }
    }

    if(online == 0U) {
        return 0U;
    }

    return online - 1U;
}

// Returns how many harts are still running
static unsigned long stop_other_harts(void)
{
    unsigned long others = count_other_harts();

    if(others == 0U || panic_ipi_number == UINT_MAX) {
        return others;
    }

    hart_ipi(HART_IPI_EXCEPT_ME, panic_ipi_number);

    unsigned long stopped = 0U;

    for(unsigned long i = 0U; i < STOP_WAIT_SPINS; ++i) {
        stopped = atomic_load_n(&stopped_harts, ATOMIC_ACQUIRE);

        if(stopped >= others) {
            return 0U;
        }

        hart_relax();
    }

    return others - stopped;
}

void __noreturn panic_at(const char* restrict file, unsigned long line, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic_at(file, line, fmt, ap);
}

void __noreturn vpanic_at(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap)
{
    disable_interrupts();

    bool expected = false;

    // A second panic, on this hart or another, would only fight the first one for the consoles
    if(!atomic_cmpxchg(&panic_claimed, &expected, true, ATOMIC_ACQ_REL, ATOMIC_RELAXED)) {
        halt_stopped();
    }

    unsigned long running_harts = stop_other_harts();

    // From here on kprintf and the consoles skip their locks
    atomic_store_n(&panic_in_progress, true, ATOMIC_RELEASE);

    console_takeover_all();

    pr_emerg("kernel panic at %s:%lu", file, line);

    kvprintf(KP_EMERG, fmt, ap);

    if(running_harts) {
        pr_emerg("panic: %lu harts did not stop", running_harts);
    }

    for(;;) {
        disable_interrupts();
        hart_spin();
    }
}

static void init_panic_ipi(void)
{
    panic_ipi_number = irq_handler_add(UINT_MAX, &panic_ipi_irq, nullptr);
}

initcall_bsp_early(init_panic_ipi, INIT_MIDDLE);
