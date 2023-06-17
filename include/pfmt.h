/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_PFMT_H__
#define __INCLUDE_PFMT_H__
#include <kan/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

typedef void (*pfmt_func_t)(int c, void *restrict arg);
int pfmt(pfmt_func_t func, void *restrict arg, const char *restrict fmt, ...) __printflike(3, 4);
int vpfmt(pfmt_func_t func, void *restrict arg, const char *restrict fmt, va_list ap);

#endif /* __INCLUDE_PFMT_H__ */
