/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/kprintf.h>
#include <sprintf.h>
#include <string.h>

#define KP_RING_SIZE 4096
static char kp_ring[KP_RING_SIZE] = { 0 };
static char kp_staging[1024] = { 0 };
static size_t kp_ring_writepos = 0;
static kp_callback_t kp_callback = NULL;
static unsigned long kp_mask = KP_EVERYTHING;

static void kp_ring_puts(const char *restrict s)
{
    size_t i;

    for(i = 0; s[i]; i++) {
        kp_ring[kp_ring_writepos++] = s[i];
        kp_ring_writepos %= KP_RING_SIZE;
    }

    if(!kp_callback)
        return;
    kp_callback(s, i);        
}

unsigned long kp_get_mask(void)
{
    return kp_mask;
}

unsigned long kp_set_mask(unsigned long mask)
{
    kp_mask = mask;
    return kp_mask;
}

void kp_set_callback(kp_callback_t callback)
{
    char *sptr;

    kp_callback = callback;

    if(kp_callback) {
        sptr = kp_ring + kp_ring_writepos;
        kp_callback(sptr, strnlen(sptr, KP_RING_SIZE - kp_ring_writepos));
        kp_callback(kp_ring, strnlen(kp_ring, kp_ring_writepos)); 
    }
}

void kputs(unsigned long source, const char *restrict s)
{
    if(source == KP_UNMASKABLE || (source & kp_mask)) {
        kp_ring_puts(s);
        kp_ring_puts("\r\n");
    }
}

void kvprintf(unsigned long source, const char *restrict fmt, va_list ap)
{
    if(source == KP_UNMASKABLE || (source & kp_mask)) {
        vsnprintf(kp_staging, sizeof(kp_staging), fmt, ap);
        kp_ring_puts(kp_staging);
        kp_ring_puts("\r\n");
    }
}

void kprintf(unsigned long source, const char *restrict fmt, ...)
{
    va_list ap;

    if(source == KP_UNMASKABLE || (source & kp_mask)) {
        va_start(ap, fmt);
        vsnprintf(kp_staging, sizeof(kp_staging), fmt, ap);
        kp_ring_puts(kp_staging);
        kp_ring_puts("\r\n");
        va_end(ap);
    }
}
