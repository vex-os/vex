/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_PANIC_H__
#define __INCLUDE_KANERU_PANIC_H__
#include <stdarg.h>
#include <kaneru/cdefs.h>

void panicv_impl(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap) __noreturn;
void panic_impl(const char *restrict file, unsigned long line, const char *restrict fmt, ...) __noreturn __format(printf, 3, 4);
#define panicv(fmt, ap) panicv_impl(__FILE__, __LINE__, (fmt), (ap))
#define panic(fmt, ...) panic_impl(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)

#endif /* __INCLUDE_KANERU_PANIC_H__ */
