/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_SPRINTF_H__
#define _INCLUDE_SPRINTF_H__
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

size_t snprintf(char *restrict s, size_t n, const char *restrict fmt, ...) __printf(3, 4) __weak;
size_t sprintf(char *restrict s, const char *restrict fmt, ...) __printf(2, 3) __weak;
size_t vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap) __weak;
size_t vsprintf(char *restrict s, const char *restrict fmt, va_list ap) __weak;

#endif /* _INCLUDE_SPRINTF_H__ */
