/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_PRINTF_H
#define _INCLUDE_SYS_PRINTF_H
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

#if !defined(KMSG_SIZE)
#define KMSG_SIZE 4096
#endif

#define KP_EMERG    0x0000
#define KP_ALERT    0x0001
#define KP_CRITICAL 0x0002
#define KP_ERROR    0x0003
#define KP_WARNING  0x0004
#define KP_NOTICE   0x0005
#define KP_INFORM   0x0006
#define KP_DEBUG    0x0007

extern unsigned loglevel;
extern size_t kmsg_writepos;
extern char kmsg[KMSG_SIZE];

void kputs(unsigned sv, const char *restrict str);
void kprintf(unsigned sv, const char *restrict fmt, ...) __printflike(2, 3);
void kvprintf(unsigned sv, const char *restrict fmt, va_list ap) __printflike(2, 0);

#endif /* _INCLUDE_SYS_PRINTF_H */
