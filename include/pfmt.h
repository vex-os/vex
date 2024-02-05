/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_PFMT_H
#define INCLUDE_PFMT_H
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

typedef void (*pfmt_func_t)(int ch, void *restrict arg);
int pfmt(pfmt_func_t func, void *restrict arg, const char *restrict fmt, ...) __printflike(3, 4);
int vpfmt(pfmt_func_t func, void *restrict arg, const char *restrict fmt, va_list ap);

#endif /* INCLUDE_PFMT_H */
