/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_DEBUG_H__
#define __INCLUDE_KANERU_DEBUG_H__
#include <stdarg.h>
#include <kaneru/cdefs.h>

void vpanic(const char *restrict fmt, va_list ap) __noreturn;
void panic(const char *restrict fmt, ...) __noreturn __format(printf, 1, 2);

#define kassert(x) ({if(!(x)){panic("assertion failed: %s",#x);}})
#define kassertf(x, fmt, ...) ({if(!(x){panic((fmt),##__VA_ARGS__);})})

#endif /* __INCLUDE_KANERU_DEBUG_H__ */
