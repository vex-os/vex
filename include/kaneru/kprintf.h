/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_KPRINTF_H__
#define __INCLUDE_KANERU_KPRINTF_H__
#include <kaneru/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define KPRINTF_BUFSIZE 8192

#define KLOG_EMERG  0
#define KLOG_ALERT  1
#define KLOG_CRIT   2
#define KLOG_ERROR  3
#define KLOG_WARN   4
#define KLOG_NOTICE 5
#define KLOG_INFORM 6
#define KLOG_DEBUG  7

extern short kprintf_level;

size_t kprintf(short level, const char *fmt, ...) __format(printf, 2, 3);
size_t kvprintf(short level, const char *fmt, va_list va);

struct syscon;
void kprintf_flush_syscon(struct syscon *console);

#define pr_emerg(fmt, ...)  kprintf(KLOG_EMERG,     (fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)  kprintf(KLOG_ALERT,     (fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...)   kprintf(KLOG_CRIT,      (fmt), ##__VA_ARGS__)
#define pr_error(fmt, ...)  kprintf(KLOG_ERROR,     (fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...)   kprintf(KLOG_WARN,      (fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...) kprintf(KLOG_NOTICE,    (fmt), ##__VA_ARGS__)
#define pr_inform(fmt, ...) kprintf(KLOG_INFORM,    (fmt), ##__VA_ARGS__)
#define pr_debug(fmt, ...)  kprintf(KLOG_DEBUG,     (fmt), ##__VA_ARGS__)

#endif /* __INCLUDE_KANERU_KPRINTF_H__ */
