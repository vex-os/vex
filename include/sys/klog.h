/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SYS_KLOG_H__
#define __INCLUDE_SYS_KLOG_H__
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

#define KL_MASK(x)  ((unsigned long)(1 << (x)))
#define KL_MASK_ALL ((unsigned long)0xFFFFFFFF)
#define KL_MASK_NIL ((unsigned long)0x00000000)

#define KL_UNDEF    (0x00) /* undefined subsystems */
#define KL_INIT     (0x01) /* kernel initialization */
#define KL_CONSOLE  (0x02) /* console subsystem */

struct console;

unsigned long klog_getmask(void);
unsigned long klog_setmask(unsigned long mask);
void klog_print_all(struct console *con);
void klogv_impl(unsigned long origin, const char *file, unsigned long line, const char *fmt, va_list va);
void klog_impl(unsigned long origin, const char *file, unsigned long line, const char *fmt, ...) __format(printf, 4, 5);
#define klogv(origin, fmt, va) klogv_impl((origin), __FILE__, __LINE__, (fmt), (va))
#define klog(origin, fmt, ...) klog_impl((origin), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)

#endif /* __INCLUDE_SYS_KLOG_H__ */
