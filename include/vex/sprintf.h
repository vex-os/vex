#ifndef F714D083_1D91_43A5_88AB_3A734D8ED3C0
#define F714D083_1D91_43A5_88AB_3A734D8ED3C0
#pragma once

#include <stdarg.h>
#include <vex/compiler.h>
#include <vex/types.h>

int snprintf(char* restrict buffer, size_t sz, const char* restrict fmt, ...) __printflike(3, 4);
int sprintf(char* restrict buffer, const char* restrict fmt, ...) __printflike(2, 3);
int vsnprintf(char* restrict buffer, size_t sz, const char* restrict fmt, va_list ap) __printflike(3, 0);
int vsprintf(char* restrict buffer, const char* restrict fmt, va_list ap) __printflike(2, 0);

#endif /* F714D083_1D91_43A5_88AB_3A734D8ED3C0 */
