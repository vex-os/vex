#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/console.h>
#include <core/kprintf.h>
#include <core/panic.h>

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

    console_unblank_all();

    pr_emerg("kernel panic at %s:%lu", file, line);

    kvprintf(KP_EMERG, fmt, ap);

    for(;;) {
        disable_interrupts();
        stop_hart();
        continue;
    }
}
