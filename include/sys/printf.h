/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_SYS_PRINTF_H
#define INCLUDE_SYS_PRINTF_H
#include <stddef.h>
#include <stdarg.h>
#include <sys/cdefs.h>

#if !defined(KMSG_SIZE)
#define KMSG_SIZE 4096
#endif

enum kp_severity {
    KP_EMERG    = 0U,
    KP_ALERT    = 1U,
    KP_CRITICAL = 2U,
    KP_ERROR    = 3U,
    KP_WARNING  = 4U,
    KP_NOTICE   = 5U,
    KP_INFORM   = 6U,
    KP_DEBUG    = 7U,
};

extern unsigned int loglevel;
extern size_t kmsg_writepos;
extern char kmsg[KMSG_SIZE];

void kputs(unsigned int sv, const char *restrict str);
void kprintf(unsigned int sv, const char *restrict fmt, ...) __printflike(2, 3);
void kvprintf(unsigned int sv, const char *restrict fmt, va_list ap) __printflike(2, 0);

#endif /* INCLUDE_SYS_PRINTF_H */
