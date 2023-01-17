/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/console.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <sprintf.h>

short kp_verbosity = KP_TRACE;
char kp_ring[KP_RING_SZ] = { 0 };
size_t kp_ring_pos = 0;

static void kp_ring_puts(const char *restrict str)
{
    size_t n;

    /* Acts as strlen of sorts */
    for(n = 0; str[n]; n++) {
        kp_ring[kp_ring_pos++] = str[n];
        kp_ring_pos %= KP_RING_SZ;
    }

    console_write(str, n);
}

void kputs(short severity, const char *restrict str)
{
    if(severity <= kp_verbosity) {
        kp_ring_puts(str);
        kp_ring_puts("\r\n");
    }
}
EXPORT_SYMBOL(kputs);

void kvprintf(short severity, const char *restrict fmt, va_list ap)
{
    static char msg[KP_MSG_SZ] = { 0 };

    if(severity <= kp_verbosity) {
        vsnprintf(msg, sizeof(msg), fmt, ap);
        kp_ring_puts(msg);
        kp_ring_puts("\r\n");
    }
}
EXPORT_SYMBOL(kvprintf);

void kprintf(short severity, const char *restrict fmt, ...)
{
    va_list ap;

    if(severity <= kp_verbosity) {
        va_start(ap, fmt);
        kvprintf(severity, fmt, ap);
        va_end(ap);
    }
}
EXPORT_SYMBOL(kprintf);
