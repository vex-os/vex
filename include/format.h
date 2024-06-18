// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_FORMAT_H
#define INCLUDE_FORMAT_H
#include <iserix/compiler.h>
#include <stdarg.h>
#include <stddef.h>

typedef void(*format_func_t)(int chr, void *restrict arg);
int format(format_func_t func, void *restrict arg, const char *restrict fmt,...) PRINTFLIKE(3, 4);
int vformat(format_func_t func, void *restrict arg, const char *restrict fmt, va_list ap) PRINTFLIKE(3, 0);

#endif /* INCLUDE_FORMAT_H */
