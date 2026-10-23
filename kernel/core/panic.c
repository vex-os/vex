#include <vex/atomic.h>
#include <vex/console.h>
#include <vex/hart.h>
#include <vex/kprintf.h>
#include <vex/panic.h>

// Somewhere around a second of pause instructions; the clock source can't time
// this, as it takes a lock that a hart which doesn't stop may be holding
#define STOP_WAIT_SPINS 0x1000000UL

volatile bool panic_in_progress = false;

static bool panic_claimed = false;
static unsigned long stopped_harts = 0U;

static void __noreturn halt_stopped(void)
{
    atomic_fetch_add(&stopped_harts, 1, ATOMIC_RELEASE);

    for(;;) {
        disable_interrupts();
        hart_spin();
    }
}

// Returns how many harts are still running; an NMI rather than an IPI, as a hart
// may wait with interrupts disabled for this one, say for a TLB shootdown it won't answer
static unsigned long stop_other_harts(void)
{
    hart_local_t* self = this_hart();
    unsigned long others = 0U;

    for(unsigned long i = 0U; i < num_harts; ++i) {
        hart_local_t* hart = &harts[i];
        unsigned short flags = atomic_load_n(&hart->flags, ATOMIC_ACQUIRE);

        if(hart != self && (flags & HART_FLAG_ONLINE)) {
            hart_nmi(hart);
            others += 1U;
        }
    }

    unsigned long stopped = 0U;

    for(unsigned long i = 0U; i < STOP_WAIT_SPINS && stopped < others; ++i) {
        stopped = atomic_load_n(&stopped_harts, ATOMIC_ACQUIRE);
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

    kvprintf(KP_SUBSYSTEM, KP_EMERG, fmt, ap);

    if(running_harts) {
        pr_emerg("panic: %lu harts did not stop", running_harts);
    }

    for(;;) {
        disable_interrupts();
        hart_spin();
    }
}

void panic_nmi(void)
{
    if(atomic_load_n(&panic_claimed, ATOMIC_ACQUIRE)) {
        halt_stopped();
    }
}
