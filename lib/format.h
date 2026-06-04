#ifndef AF47BF7C_48CD_4CD6_A12B_4EC3DE881CA5
#define AF47BF7C_48CD_4CD6_A12B_4EC3DE881CA5
#pragma once

#include <core/cdefs.h>
#include <core/types.h>
#include <stdarg.h>

typedef void (*format_func_type)(int character, void* restrict user);

int format(format_func_type func, void* restrict user, const char* restrict fmt, ...) __printflike(3, 4);
int vformat(format_func_type func, void* restrict user, const char* restrict fmt, va_list args) __printflike(3, 0);

#endif /* AF47BF7C_48CD_4CD6_A12B_4EC3DE881CA5 */
