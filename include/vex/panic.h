/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_VEX_PANIC_H
#define INCLUDE_VEX_PANIC_H
#include <vex/compiler.h>
#include <stdarg.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...) NORETURN PRINTFLIKE(3, 4);
void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap) NORETURN;
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fpanic(__FILE__, __LINE__, (fmt), (ap))

#endif /* INCLUDE_VEX_PANIC_H */
