/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/kprintf.h>
#include <kan/syscon.h>
#include <sprintf.h>

short kp_loglevel = KP_TRACE;
unsigned long kp_mask = KP_MASK_NONE;
char kp_ring[KP_RING_SIZE] = { 0 };
size_t kp_ring_pos = 0;

static char kp_staging[KP_MSG_SIZE] = { 0 };

static void kp_ring_puts(const char *restrict s)
{
    size_t n;

    /* Acts as strlen of sorts */
    for(n = 0; s[n]; n++) {
        kp_ring[kp_ring_pos++] = s[n];
        kp_ring_pos %= KP_RING_SIZE;
    }

    syscon_write(s, n);
}

void kputs(short level, unsigned long source, const char *restrict s)
{
    if(level <= kp_loglevel && (source == KP_UNMASKABLE || (source & kp_mask))) {
        kp_ring_puts(s);
        kp_ring_puts("\r\n");
    }
}

void kvprintf(short level, unsigned long source, const char *restrict fmt, va_list ap)
{
    if(level <= kp_loglevel && (source == KP_UNMASKABLE || (source & kp_mask))) {
        vsnprintf(kp_staging, sizeof(kp_staging), fmt, ap);
        kp_ring_puts(kp_staging);
        kp_ring_puts("\r\n");
    }
}

void kprintf(short level, unsigned long source, const char *restrict fmt, ...)
{
    va_list ap;

    if(level <= kp_loglevel && (source == KP_UNMASKABLE || (source & kp_mask))) {
        va_start(ap, fmt);
        vsnprintf(kp_staging, sizeof(kp_staging), fmt, ap);
        va_end(ap);
        kp_ring_puts(kp_staging);
        kp_ring_puts("\r\n");
    }
}
