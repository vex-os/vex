#pragma once

#include <stdarg.h>
#include <vex/compiler.h>
#include <vex/types.h>

typedef void (*format_func_t)(int character, void* restrict user);

int format(format_func_t func, void* restrict user, const char* restrict fmt, ...) __printflike(3, 4);
int vformat(format_func_t func, void* restrict user, const char* restrict fmt, va_list args) __printflike(3, 0);
