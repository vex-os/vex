/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_SPRINTF_H__
#define _INCLUDE_SPRINTF_H__
#include <kan/compiler.h>
#include <stdarg.h>
#include <stddef.h>

size_t snprintf(char *restrict s, size_t n, const char *restrict fmt, ...) __printf(3, 4);
size_t sprintf(char *restrict s, const char *restrict fmt, ...) __printf(2, 3);
size_t vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap);
size_t vsprintf(char *restrict s, const char *restrict fmt, va_list ap);

#endif /* _INCLUDE_SPRINTF_H__ */
