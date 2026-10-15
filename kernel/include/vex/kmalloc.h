#pragma once

#include <stdarg.h>
#include <vex/compiler.h>
#include <vex/types.h>

void* kmalloc(size_t sz) __nodiscard;
void* krealloc(void* restrict ptr, size_t sz) __nodiscard;
void kfree(void* restrict ptr);

char* kstrdup(const char* restrict string) __nodiscard;
char* kasprintf(const char* restrict fmt, ...) __nodiscard __printflike(1, 2);
char* kvasprintf(const char* restrict fmt, va_list ap) __nodiscard __printflike(1, 0);
