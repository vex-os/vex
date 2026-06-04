#ifndef CD002577_1397_4B49_80D1_C5425889EB62
#define CD002577_1397_4B49_80D1_C5425889EB62
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

void* kmalloc(size_t sz) __nodiscard;
void* kcalloc(size_t count, size_t sz) __nodiscard;
void* krealloc(void* restrict ptr, size_t sz) __nodiscard;
void kfree(void* restrict ptr);

char* kstrdup(const char* restrict string) __nodiscard;

#endif /* CD002577_1397_4B49_80D1_C5425889EB62 */
