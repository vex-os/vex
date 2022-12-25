/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/cpu.h>
#include <kan/debug.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <limits.h>
#include <sprintf.h>

void __noreturn vpanic(const char *restrict fmt, va_list ap)
{
    static char msg[KP_MSG_SIZE] = { 0 };

    cpu_disable_interrupts();

    /* Print EVERYTHNG */
    kp_loglevel = SHRT_MAX;
    kp_mask = KP_MASK_NONE;

    vsnprintf(msg, sizeof(msg), fmt, ap);
    pr_emerg(KP_UNMASKABLE, "kernel panic: %s", msg);

    /* Print backtrace with the same
     * loglevel as the panic message */
    print_backtrace(KP_EMERG, KP_UNMASKABLE, NULL);

    for(;;) {
        cpu_idle();
        cpu_disable_interrupts();
    }

    unreachable();
}

void __noreturn panic(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpanic(fmt, ap);
    va_end(ap);

    unreachable();
}

void print_backtrace(short level, unsigned long source, const uintptr_t *restrict baseptr)
{
    size_t i;
    uintptr_t next;
    uintptr_t rptr;
    ptrdiff_t off;
    symbol_t sym;

    if(!baseptr) {
        baseptr = cpu_get_baseptr();

        if(!baseptr) {
            /* Not available */
            return;
        }
    }

    for(i = 0;; i++) {
        next = baseptr[0];
        rptr = baseptr[1];

        if(next == 0 || rptr == 0) {
            /* Any of these being zero indicates end of trace*/
            break;
        }

        if(trace_address(rptr, &sym, &off)) {
            /* Traceable symbol found */
            kprintf(level, source, "#%02zX [%p] %s(%#02tX)", i, (void *)rptr, sym.name, off);
        }
        else {
            /* Untraceable address, possibly within a module... */
            kprintf(level, source, "#%02zX [%p]", i, (void *)rptr);
        }

        baseptr = (const uintptr_t *)next;
    }
}
