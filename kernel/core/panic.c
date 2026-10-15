#include <vex/atomic.h>
#include <vex/console.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/panic.h>

volatile bool panic_in_progress = false;

static unsigned panic_ipi_number = UINT_MAX;

static void panic_ipi_ireq(unsigned number, const struct hart_context* restrict regs, void* restrict ctxdata)
{
    disable_interrupts();

    struct hart_local* hart = this_hart();

    pr_emerg("panic: stopping hart %hu", hart->index);

    for(;;) {
        hart_spin();
    }
}

void __noreturn fpanic(const char* restrict file, unsigned long line, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
}

void __noreturn fvpanic(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap)
{
    console_unblank();

    pr_emerg("kernel panic at %s:%lu", file, line);

    if(panic_ipi_number < UINT_MAX) {
        hart_ipi(HART_IPI_EXCEPT_ME, panic_ipi_number);
    }

    disable_interrupts();

    atomic_store_n(&panic_in_progress, true, ATOMIC_RELEASE);

    kvprintf(KP_EMERG, fmt, ap);

    for(;;) {
        disable_interrupts();
        hart_spin();
        continue;
    }
}

static void init_panic_ipi(void)
{
    panic_ipi_number = add_ireq_handler(UINT_MAX, &panic_ipi_ireq, nullptr);
}

initcall_bsp_early(init_panic_ipi, INIT_MIDDLE);
