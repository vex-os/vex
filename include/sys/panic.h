/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_PANIC_H__
#define __INCLUDE_SYS_PANIC_H__
#include <stdarg.h>
#include <sys/cdefs.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...) __printflike(3, 4) __noreturn;
void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap) __printflike(3, 0) __noreturn;
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic(__FILE__, __LINE__, (fmt), (ap))
#define panic_if(x, fmt, ...) ({if(predict_false(x)){panic(fmt, ##__VA_ARGS__);unreachable();}})

#define kassert(x) ({if(predict_false(!(x))){panic("assertion failed - %s", #x); unreachable();}})

#endif /* __INCLUDE_SYS_PANIC_H__ */
