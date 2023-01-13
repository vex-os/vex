/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_DEBUG_H__
#define _INCLUDE_KAN_DEBUG_H__
#include <kan/compiler.h>
#include <stdarg.h>
#include <stdint.h>

void vpanic(const char *restrict fmt, va_list ap) __noreturn;
void panic(const char *restrict fmt, ...) __printf(1, 2) __noreturn;
void kprint_backtrace(short severity, const uintptr_t *restrict baseptr);

#define kassert(x) ({if(__unlikely(!(x))){panic("assertion failed: %s", #x);unreachable();}})
#define kassertf(x, fmt, ...) ({if(__unlikely(!(x))){panic((fmt), ##__VA_ARGS__);unreachable();}})
#define panic_if(x, fmt, ...) ({if(__unlikely((x))){panic((fmt), ##__VA_ARGS__);unreachable();}})

#endif /* _INCLUDE_KAN_DEBUG_H__ */
