/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_DEBUG_H__
#define __INCLUDE_SYS_DEBUG_H__
#include <stdarg.h>
#include <sys/cdefs.h>

void fpanic(const char *restrict file, unsigned long line, const char *restrict fmt, ...) __noreturn __printflike(3, 4);
void fvpanic(const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap) __noreturn;
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic(__FILE__, __LINE__, (fmt), (ap))
#define kassert(x) ({if_unlikely(!(x)){panic("assertion failed - %s", #x); UNREACHABLE();}})
#define kassertf(x, fmt, ...) ({if_unlikely(!(x)){panic((fmt), ##__VA_ARGS__); UNREACHABLE();}})

#endif /* __INCLUDE_SYS_DEBUG_H__ */
