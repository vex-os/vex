/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/console.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <sprintf.h>

short kp_verbosity = KP_TRACE;
char kp_ring[KP_RING_SZ] = { 0 };
size_t kp_ring_pos = 0;

static char kp_staging[KP_MSG_SZ] = { 0 };

static void kp_ring_puts(const char *restrict s)
{
    size_t n;

    /* Acts as strlen of sorts */
    for(n = 0; s[n]; n++) {
        kp_ring[kp_ring_pos++] = s[n];
        kp_ring_pos %= KP_RING_SZ;
    }

    console_write(s, n);
}

void kputs(short severity, const char *restrict s)
{
    if(severity <= kp_verbosity) {
        kp_ring_puts(s);
        kp_ring_puts("\r\n");
    }
}
EXPORT_SYMBOL(kputs);

void kvprintf(short severity, const char *restrict fmt, va_list ap)
{
    if(severity <= kp_verbosity) {
        vsnprintf(kp_staging, sizeof(kp_staging), fmt, ap);
        kp_ring_puts(kp_staging);
        kp_ring_puts("\r\n");
    }
}
EXPORT_SYMBOL(kvprintf);

void kprintf(short severity, const char *restrict fmt, ...)
{
    va_list ap;

    if(severity <= kp_verbosity) {
        va_start(ap, fmt);
        vsnprintf(kp_staging, sizeof(kp_staging), fmt, ap);
        va_end(ap);
        kp_ring_puts(kp_staging);
        kp_ring_puts("\r\n");
    }
}
EXPORT_SYMBOL(kprintf);
