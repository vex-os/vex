/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_KPRINTF_H__
#define _INCLUDE_KAN_KPRINTF_H__
#include <kan/compiler.h>
#include <stdarg.h>
#include <stddef.h>

#define KP_MSG_SZ   0x200
#define KP_RING_SZ  0x1000

#define KP_TRACE    50
#define KP_DEBUG    25
#define KP_NOTICE   10
#define KP_INFORM    0
#define KP_WARNING -10
#define KP_ERROR   -25
#define KP_EMERG   -50

extern short kp_verbosity;
extern char kp_ring[KP_RING_SZ];
extern size_t kp_ring_pos;

void kputs(short severity, const char *restrict str);
void kvprintf(short severity, const char *restrict fmt, va_list ap);
void kprintf(short severity, const char *restrict fmt, ...) __printf(2, 3);

#define pr_trace(fmt, ...) kprintf(KP_TRACE, (fmt), ##__VA_ARGS__)
#define pr_debug(fmt, ...) kprintf(KP_DEBUG, (fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...) kprintf(KP_NOTICE, (fmt), ##__VA_ARGS__)
#define pr_inform(fmt, ...) kprintf(KP_INFORM, (fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) kprintf(KP_WARNING, (fmt), ##__VA_ARGS__)
#define pr_error(fmt, ...) kprintf(KP_ERROR, (fmt), ##__VA_ARGS__)
#define pr_emerg(fmt, ...) kprintf(KP_EMERG, (fmt), ##__VA_ARGS__)

#endif /* _INCLUDE_KAN_KPRINTF_H__ */
