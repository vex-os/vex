/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KAN_DEBUG_H__
#define __INCLUDE_KAN_DEBUG_H__
#include <kan/cdefs.h>
#include <stdarg.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...) __noreturn __printflike(3, 4);
void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap) __noreturn __printflike(3, 0);
#define panic(fmt, ...) fpanic((__FILE__), (__LINE__), (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ...) fvpanic((__FILE__), (__LINE__), (fmt), (ap))

#define kassert(x) ({if(UNLIKELY(!(x))){panic("assertion failed - %s", #x); UNREACHABLE();}})
#define kassertf(x, fmt, ...) ({if(UNLIKELY(!(x))){panic((fmt), ##__VA_ARGS__); UNREACHABLE();}})
#define panic_if(x, fmt, ...) ({if(UNLIKELY(x)){panic((fmt), ##__VA_ARGS__); UNREACHABLE();}})

#endif /* __INCLUDE_KAN_DEBUG_H__ */
