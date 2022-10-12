/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SPRINTF_H__
#define __INCLUDE_SPRINTF_H__
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...) __format(printf, 3, 4) __weak;
int sprintf(char *restrict s, const char *restrict fmt, ...) __format(printf, 2, 3) __weak;
int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list va) __weak;
int vsprintf(char *restrict s, const char *restrict fmt, va_list va) __weak;

#endif /* __INCLUDE_SPRINTF_H__ */
