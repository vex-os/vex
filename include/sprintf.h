/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SPRINTF_H
#define _INCLUDE_SPRINTF_H
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

int snprintf(char *restrict str, size_t sz, const char *restrict fmt, ...) __printflike(3, 4);
int sprintf(char *restrict str, const char *restrict fmt, ...) __printflike(2, 3);
int vsnprintf(char *restrict str, size_t sz, const char *restrict fmt, va_list ap) __printflike(3, 0);
int vsprintf(char *restrict str, const char *restrict fmt, va_list ap) __printflike(2, 0);

#endif /* _INCLUDE_SPRINTF_H */
