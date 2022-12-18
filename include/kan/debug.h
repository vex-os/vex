/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_KAN_DEBUG_H__
#define _INCLUDE_KAN_DEBUG_H__
#include <kan/compiler.h>
#include <stdarg.h>
#include <stdint.h>

void noreturn vpanic(const char *restrict fmt, va_list ap);
void noreturn panic(const char *restrict fmt, ...) __printf(1, 2);
void print_backtrace(short level, unsigned long source, const uintptr_t *restrict baseptr);

#define kassert(x) ({if(!(x))panic("assertion failed: %s",#x);})
#define kassertf(x, fmt, ...) ({if(!(x))panic((fmt),##__VA_ARGS__);})

#endif /* _INCLUDE_KAN_DEBUG_H__ */
