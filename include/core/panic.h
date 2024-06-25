/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_CORE_PANIC_H
#define INCLUDE_CORE_PANIC_H
#include <core/compiler.h>
#include <stdarg.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...) __noreturn __printflike(3, 4);
void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap) __noreturn __printflike(3, 0);
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic(__FILE__, __LINE__, (fmt), (ap))

#endif /* INCLUDE_CORE_PANIC_H */
