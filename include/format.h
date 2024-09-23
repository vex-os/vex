/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef INCLUDE_FORMAT_H
#define INCLUDE_FORMAT_H
#include <kern/compiler.h>
#include <stdarg.h>
#include <stddef.h>

typedef void(*format_func_t)(int chr, void *restrict arg);
int format(format_func_t func, void *restrict arg, const char *restrict fmt,...) __printflike(3, 4);
int vformat(format_func_t func, void *restrict arg, const char *restrict fmt, va_list ap) __printflike(3, 0);

#endif /* INCLUDE_FORMAT_H */
