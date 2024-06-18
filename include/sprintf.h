// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_SPRINTF_H
#define INCLUDE_SPRINTF_H
#include <iserix/compiler.h>
#include <stdarg.h>
#include <stddef.h>

int snprintf(char *restrict str, size_t sz, const char *restrict fmt, ...) PRINTFLIKE(3, 4);
int sprintf(char *restrict str, const char *restrict fmt, ...) PRINTFLIKE(2, 3);
int vsnprintf(char *restrict str, size_t sz, const char *restrict fmt, va_list ap) PRINTFLIKE(3, 0);
int vsprintf(char *restrict str, const char *restrict fmt, va_list ap) PRINTFLIKE(2, 0);

#endif /* INCLUDE_SPRINTF_H */
