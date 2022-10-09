/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_DEBUG_H__
#define __INCLUDE_KANERU_DEBUG_H__
#include <kaneru/cdefs.h>
#include <stdarg.h>

void panicv(const char *fmt, va_list va) __noreturn;
void panic(const char *fmt, ...) __noreturn __format(printf, 1, 2);

#endif /* __INCLUDE_KANERU_DEBUG_H__ */
