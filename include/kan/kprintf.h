/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KAN_KPRINTF_H__
#define __INCLUDE_KAN_KPRINTF_H__
#include <kan/cdefs.h>
#include <stdarg.h>

void kputs(const char *restrict s);
void kprintf(const char *restrict fmt, ...) __printflike(1, 2);
void kvprintf(const char *restrict fmt, va_list ap) __printflike(1, 0);

#endif /* __INCLUDE_KAN_KPRINTF_H__ */
