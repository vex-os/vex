/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STDARG_H__
#define __INCLUDE_STDARG_H__

typedef __builtin_va_list va_list;
#define va_start(va, x) __builtin_va_start(va, x)
#define va_copy(va, dv) __builtin_va_copy(va, dv)
#define va_arg(va, t)   __builtin_va_arg(va, t)
#define va_end(va)      __builtin_va_end(va)

#endif /* __INCLUDE_STDARG_H__ */
