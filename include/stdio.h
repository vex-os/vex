/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...) __printflike(3, 4);
int sprintf(char *restrict s, const char *restrict fmt, ...) __printflike(2, 3);
int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap);
int vsprintf(char *restrict s, const char *restrict fmt, va_list ap);

#endif /* INCLUDE_STDIO_H */
