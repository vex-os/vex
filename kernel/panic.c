#include <arch/halt.h>
#include <arch/intr.h>

#include <vex/consdrv.h>
#include <vex/kprintf.h>
#include <vex/panic.h>

void __noreturn fpanic(const char* restrict file, unsigned long line, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    unreachable();
    va_end(ap);
}

void __noreturn fvpanic(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap)
{
    disable_interrupts();

    consdrv_unblank();

    kprintf(KP_EMERG, "panic: at %s:%lu", file, line);
    kvprintf(KP_EMERG, fmt, ap);

    // UNDONE: at least try to get backtarce
    // at this point? Core should have this subsystem
    // in place alongside kernel symbol name resolver

    for(;;) {
        disable_interrupts();
        halt_cpu();
    }

    unreachable();
}
