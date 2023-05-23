/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <machine/cpu.h>
#include <sys/klog.h>
#include <sys/panic.h>

void __noreturn fpanic(const char *restrict file, long line, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fvpanic(file, line, fmt, ap);
    UNREACHABLE();
    va_end(ap);
}

void __noreturn fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap)
{
    klog_sink_t *it;

    cpu_disable_interrupts();

    for(it = klog_sinks; it; it = it->next) {
        if(!it->unblank)
            continue;
        it->unblank(it);
    }

    kvlog(LOG_EMERG, "\n!~ KERNEL PANIC ~!\n", ap);
    klog(LOG_EMERG, "panic at [%s:%ld]", file, line);
    kvlog(LOG_EMERG, fmt, ap);

    /* UNDONE: backtrace */
    /* print_backtrace(NULL); */

    for(;;) {
        cpu_idle();
        cpu_disable_interrupts();
        continue;
    }

    UNREACHABLE();
}
