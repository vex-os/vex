/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#ifndef INCLUDE_KERNEL_PANIC_H
#define INCLUDE_KERNEL_PANIC_H
#include <stdarg.h>
#include <vx/cdefs.h>

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...) __printflike(3, 4) __noreturn;
void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap) __printflike(3, 0) __noreturn;
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic(__FILE__, __LINE__, (fmt), (ap))

#endif /* INCLUDE_KERNEL_PANIC_H */
