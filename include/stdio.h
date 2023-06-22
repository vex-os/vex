/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_STDIO_H__
#define __INCLUDE_STDIO_H__
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...) __printflike(3, 4);
int sprintf(char *restrict s, const char *restrict fmt, ...) __printflike(2, 3);
int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap);
int vsprintf(char *restrict s, const char *restrict fmt, va_list ap);

#endif /* __INCLUDE_STDIO_H__ */
