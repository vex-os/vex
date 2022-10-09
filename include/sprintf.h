/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SPRINTF_H__
#define __INCLUDE_SPRINTF_H__
#include <kaneru/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

/* formatted output to a fixed-length string */
size_t snprintf(char *restrict s, size_t n, const char *restrict fmt, ...) __format(printf, 3, 4) __weak;

/* formatted output to a string */
size_t sprintf(char *restrict s, const char *restrict fmt, ...) __format(printf, 2, 3) __weak;

/* formatted output to a fixed-length string */
size_t vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list va) __weak;

/* formatted output to a string */
size_t vsprintf(char *restrict s, const char *restrict fmt, va_list va) __weak;

#endif /* __INCLUDE_SPRINTF_H__ */
