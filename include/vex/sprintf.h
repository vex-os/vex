#ifndef _VEX_SPRINTF_H
#define _VEX_SPRINTF_H 1

#include <vex/compiler.h>

#include <stdarg.h>
#include <stddef.h>

extern int snprintf(char* restrict buffer, size_t sz, const char* restrict fmt, ...) __printflike(3, 4);
extern int sprintf(char* restrict buffer, const char* restrict fmt, ...) __printflike(2, 3);
extern int vsnprintf(char* restrict buffer, size_t sz, const char* restrict fmt, va_list ap) __printflike(3, 0);
extern int vsprintf(char* restrict buffer, const char* restrict fmt, va_list ap) __printflike(2, 0);

#endif
