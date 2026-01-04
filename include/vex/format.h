#ifndef _VEX_FORMAT_H
#define _VEX_FORMAT_H 1

#include <vex/compiler.h>

#include <stdarg.h>
#include <stddef.h>

typedef void (*format_func_t)(int character, void* restrict user);

extern int format(format_func_t func, void* restrict user, const char* restrict fmt, ...) __printflike(3, 4);
extern int vformat(format_func_t func, void* restrict user, const char* restrict fmt, va_list args) __printflike(3, 0);

#endif
