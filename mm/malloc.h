#ifndef CD002577_1397_4B49_80D1_C5425889EB62
#define CD002577_1397_4B49_80D1_C5425889EB62
#pragma once

#include <core/cdefs.h>
#include <stddef.h>

extern void* kmalloc(size_t sz) __nodiscard;
extern void* kcalloc(size_t count, size_t sz) __nodiscard;
extern void* krealloc(void* restrict ptr, size_t sz) __nodiscard;
extern void kfree(void* restrict ptr);

#endif /* CD002577_1397_4B49_80D1_C5425889EB62 */
