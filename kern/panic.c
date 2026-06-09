#include <vex.arch/hart.h>
#include <vex.arch/intr.h>
#include <vex/console.h>
#include <vex/kprintf.h>
#include <vex/panic.h>

void __noreturn fpanic(const char* restrict file, unsigned long line, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
}

void __noreturn fvpanic(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap)
{
    broadcast_panic_ipi();

    disable_interrupts();

    console_unblank();

    pr_emerg("kernel panic at %s:%lu", file, line);

    kvprintf(KP_EMERG, fmt, ap);

    for(;;) {
        disable_interrupts();
        stop_hart();
        continue;
    }
}
