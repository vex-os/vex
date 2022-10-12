/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_KPRINTF_H__
#define __INCLUDE_KANERU_KPRINTF_H__
#include <kaneru/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define KP_EMERG    1
#define KP_ALERT    2
#define KP_CRIT     3
#define KP_ERROR    4
#define KP_WARN     5
#define KP_NOTICE   6
#define KP_INFORM   7
#define KP_DEBUG    8

typedef size_t (*kp_callback_t)(const void *s, size_t n, void *arg);

/* todo: commandline */
extern unsigned short kp_level;

void kprintf_set_callback(kp_callback_t callback, void *arg);
size_t kprintf(unsigned short level, const char *fmt, ...) __format(printf, 2, 3);
size_t kvprintf(unsigned short level, const char *fmt, va_list va);

#define pr_emerg(fmt, ...)  kprintf(KP_EMERG,   (fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)  kprintf(KP_ALERT,   (fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...)   kprintf(KP_CRIT,    (fmt), ##__VA_ARGS__)
#define pr_error(fmt, ...)  kprintf(KP_ERROR,   (fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...)   kprintf(KP_WARN,    (fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...) kprintf(KP_NOTICE,  (fmt), ##__VA_ARGS__)
#define pr_inform(fmt, ...) kprintf(KP_INFORM,  (fmt), ##__VA_ARGS__)
#define pr_debug(fmt, ...)  kprintf(KP_DEBUG,   (fmt), ##__VA_ARGS__)

#endif /* __INCLUDE_KANERU_KPRINTF_H__ */
