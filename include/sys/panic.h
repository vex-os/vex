/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_PANIC_H
#define _INCLUDE_SYS_PANIC_H
#include <stdarg.h>
#include <sys/cdefs.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...) __printflike(3, 4) __noreturn;
void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap) __printflike(3, 0) __noreturn;
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic(__FILE__, __LINE__, (fmt), (ap))

#endif /* _INCLUDE_SYS_PANIC_H */
