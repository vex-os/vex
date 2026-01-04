#ifndef _VEX_PANIC_H
#define _VEX_PANIC_H 1

#include <vex/compiler.h>

#include <stdarg.h>

extern void fpanic(const char* restrict file, unsigned long line, const char* restrict fmt, ...) __noreturn __printflike(3, 4);
extern void fvpanic(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap) __noreturn __printflike(3, 0);
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic(__FILE__, __LINE__, (fmt), (ap))

#endif
