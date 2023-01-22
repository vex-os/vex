/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/console.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <sprintf.h>
#include <string.h>

short kp_verbosity = KP_TRACE;
kp_msg_t kp_history[KP_HISTORY_SZ] = { 0 };
size_t kp_msg_count = 0;

static void kp_ring_puts(const char *restrict str)
{
    size_t i;

    /* Append to the list */
    kstrncpy(kp_history[kp_msg_count++], str, sizeof(kp_msg_t));

    /* Discard an older message */
    if(kp_msg_count >= KP_HISTORY_SZ) {
        for(i = 1; i < KP_HISTORY_SZ; i++)
            memcpy(&kp_history[i - 1], &kp_history[i], sizeof(kp_msg_t));
        kp_msg_count = KP_HISTORY_SZ - 1;
    }

    console_puts(str);
}

void kputs(short severity, const char *restrict str)
{
    if(severity > kp_verbosity)
        return;
    kp_ring_puts(str);
}
EXPORT_SYMBOL(kputs);

void kvprintf(short severity, const char *restrict fmt, va_list ap)
{
    static char msg[KP_MSG_SZ] = { 0 };

    if(severity <= kp_verbosity) {
        vsnprintf(msg, sizeof(msg), fmt, ap);
        kp_ring_puts(msg);
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
