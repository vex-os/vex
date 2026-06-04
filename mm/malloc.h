#ifndef CD002577_1397_4B49_80D1_C5425889EB62
#define CD002577_1397_4B49_80D1_C5425889EB62
#pragma once

#include <core/cdefs.h>
#include <core/types.h>
#include <stdarg.h>

void* kmalloc(size_t sz) __nodiscard;
void* kcalloc(size_t count, size_t sz) __nodiscard;
void* krealloc(void* restrict ptr, size_t sz) __nodiscard;
void kfree(void* restrict ptr);

char* kstrdup(const char* restrict string) __nodiscard;

char* kasprintf(const char* restrict fmt, ...) __nodiscard __printflike(1, 2);
char* kvasprintf(const char* restrict fmt, va_list ap) __nodiscard __printflike(1, 0);

#endif /* CD002577_1397_4B49_80D1_C5425889EB62 */
