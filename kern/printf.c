/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <kern/console.h>
#include <kern/printf.h>
#include <pfmt.h>

#define LF 0x0A
#define CR 0x0D

unsigned int loglevel = KP_DEBUG;
size_t kmsg_writepos = 0;
char kmsg[KMSG_SIZE] = { 0 };

static void kp_pfmt(int ch, void *restrict arg)
{
    console_putchar(ch);
    kmsg[kmsg_writepos++] = ch;
    kmsg_writepos %= KMSG_SIZE;
}

void kputs(unsigned int sv, const char *restrict str)
{
    size_t i;

    if(sv <= loglevel) {
        for(i = 0; str[i]; kp_pfmt(str[i], NULL));
        kp_pfmt(CR, NULL);
        kp_pfmt(LF, NULL);
    }
}

void kprintf(unsigned int sv, const char *restrict fmt, ...)
{
    va_list ap;

    if(sv <= loglevel) {
        va_start(ap, fmt);
        vpfmt(&kp_pfmt, NULL, fmt, ap);
        kp_pfmt(CR, NULL);
        kp_pfmt(LF, NULL);
        va_end(ap);
    }
}

void kvprintf(unsigned int sv, const char *restrict fmt, va_list ap)
{
    if(sv <= loglevel) {
        vpfmt(&kp_pfmt, NULL, fmt, ap);
        kp_pfmt(CR, NULL);
        kp_pfmt(LF, NULL);
    }
}
