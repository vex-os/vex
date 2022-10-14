/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STDARG_H__
#define __INCLUDE_STDARG_H__

typedef __builtin_va_list va_list;

#define va_arg(ap, type) \
    __builtin_va_arg(ap, type)

#define va_copy(dest, src) \
    __builtin_va_copy(dest, src)

#define va_end(ap) \
    __builtin_va_end(ap)

#define va_start(ap, parm) \
    __builtin_va_start(ap, parm)

#endif /* __INCLUDE_STDARG_H__ */
