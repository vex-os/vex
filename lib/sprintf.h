#ifndef F714D083_1D91_43A5_88AB_3A734D8ED3C0
#define F714D083_1D91_43A5_88AB_3A734D8ED3C0
#pragma once

#include <core/cdefs.h>
#include <core/types.h>
#include <stdarg.h>

extern int snprintf(char* restrict buffer, size_t sz, const char* restrict fmt, ...) __printflike(3, 4);
extern int sprintf(char* restrict buffer, const char* restrict fmt, ...) __printflike(2, 3);
extern int vsnprintf(char* restrict buffer, size_t sz, const char* restrict fmt, va_list ap) __printflike(3, 0);
extern int vsprintf(char* restrict buffer, const char* restrict fmt, va_list ap) __printflike(2, 0);

#endif /* F714D083_1D91_43A5_88AB_3A734D8ED3C0 */
