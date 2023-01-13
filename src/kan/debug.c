/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/cpu.h>
#include <kan/debug.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <limits.h>
#include <sprintf.h>
#include <string.h>

void __noreturn vpanic(const char *restrict fmt, va_list ap)
{
    static char msg[KP_MSG_SZ] = { 0 };

    cpu_disable_interrupts();

    memset(msg, 0, sizeof(msg));
    vsnprintf(msg, sizeof(msg), fmt, ap);

    kp_verbosity = SHRT_MAX;
    kprint_backtrace(SHRT_MIN, NULL);
    kprintf(SHRT_MIN, "panic: %s", msg);

    for(;;) {
        cpu_idle();
        cpu_disable_interrupts();
    }

    unreachable();
}
EXPORT_SYMBOL(vpanic);

void __noreturn panic(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic(fmt, ap);
    unreachable();
    va_end(ap);
}
EXPORT_SYMBOL(panic);

void kprint_backtrace(short severity, const uintptr_t *restrict baseptr)
{
    uintptr_t next;
    uintptr_t rptr;
    ptrdiff_t off;
    symbol_t sym;

    if(!baseptr) {
        baseptr = cpu_get_baseptr();

        if(!baseptr) {
            kputs(severity, "backtrace: not available");
            return;
        }
    }

    for(;;) {
        next = baseptr[0];
        rptr = baseptr[1];
        if(next && rptr) {
            if(trace_address(rptr, &sym, &off))
                kprintf(severity, "backtrace: %p <%s+%#03tX>", (void *)rptr, sym.name, off);
            else
                kprintf(severity, "backtrace: %p <unknown>", (void *)rptr);
            baseptr = (const uintptr_t *)next;
            continue;
        }

        break;
    }
}
EXPORT_SYMBOL(kprint_backtrace);
