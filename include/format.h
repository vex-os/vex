/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_FORMAT_H
#define _INCLUDE_FORMAT_H
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

typedef void(*format_func_t)(int chr, void *restrict arg);
int format(format_func_t func, void *restrict arg, const char *restrict fmt,...) __printflike(3, 4);
int vformat(format_func_t func, void *restrict arg, const char *restrict fmt, va_list ap) __printflike(3, 0);

#endif /* _INCLUDE_FORMAT_H */
