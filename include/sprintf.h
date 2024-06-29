// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_SPRINTF_H
#define INCLUDE_SPRINTF_H
#include <kern/compiler.h>
#include <stdarg.h>
#include <stddef.h>

int snprintf(char *restrict str, size_t sz, const char *restrict fmt, ...) __printflike(3, 4);
int sprintf(char *restrict str, const char *restrict fmt, ...) __printflike(2, 3);
int vsnprintf(char *restrict str, size_t sz, const char *restrict fmt, va_list ap) __printflike(3, 0);
int vsprintf(char *restrict str, const char *restrict fmt, va_list ap) __printflike(2, 0);

#endif // INCLUDE_SPRINTF_H
