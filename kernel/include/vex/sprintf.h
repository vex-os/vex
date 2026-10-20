#pragma once

#include <stdarg.h>
#include <vex/compiler.h>
#include <vex/types.h>

int snprintf(char* restrict buffer, size_t size, const char* restrict fmt, ...) __printflike(3, 4);
int sprintf(char* restrict buffer, const char* restrict fmt, ...) __printflike(2, 3);
int vsnprintf(char* restrict buffer, size_t size, const char* restrict fmt, va_list ap) __printflike(3, 0);
int vsprintf(char* restrict buffer, const char* restrict fmt, va_list ap) __printflike(2, 0);
