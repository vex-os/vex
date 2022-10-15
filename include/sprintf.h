/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_PSPRINTF_H__
#define __INCLUDE_PSPRINTF_H__
#include <kaneru/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...) __format(printf, 3, 4) __weak;
int sprintf(char *restrict s, const char *restrict fmt, ...) __format(printf, 2, 3) __weak;
int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap) __weak;
int vsprintf(char *restrict s, const char *restrict fmt, va_list ap) __weak;

#endif /* __INCLUDE_PSPRINTF_H__ */
