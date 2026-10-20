#include <vex/atomic.h>
#include <vex/console.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/irq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/panic.h>

volatile bool panic_in_progress = false;

static unsigned panic_ipi_number = UINT_MAX;

static void panic_ipi_irq(unsigned number, const hart_context_t* restrict regs, void* restrict ctxdata)
{
    disable_interrupts();

    hart_local_t* hart = this_hart();

    pr_emerg("panic: stopping hart %hu", hart->index);

    for(;;) {
        hart_spin();
    }
}

void __noreturn panic_at(const char* restrict file, unsigned long line, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic_at(file, line, fmt, ap);
}

void __noreturn vpanic_at(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap)
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
    panic_ipi_number = irq_handler_add(UINT_MAX, &panic_ipi_irq, nullptr);
}

initcall_bsp_early(init_panic_ipi, INIT_MIDDLE);
