#pragma once

#include <stdarg.h>
#include <vex/compiler.h>

extern volatile bool panic_in_progress;

void panic_at(const char* restrict file, unsigned long line, const char* restrict fmt, ...) __noreturn __printflike(3, 4);
void vpanic_at(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap) __noreturn __printflike(3, 0);
#define panic(fmt, ...)  panic_at(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define vpanic(fmt, ...) vpanic_at(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
