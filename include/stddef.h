/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STDDEF_H__
#define __INCLUDE_STDDEF_H__

#ifndef __LIBC_SIZE_T_DEFINED__
#define __LIBC_SIZE_T_DEFINED__
typedef __SIZE_TYPE__ size_t;
#endif

#ifndef __LIBC_SSIZE_T_DEFINED__
#define __LIBC_SSIZE_T_DEFINED__
typedef __INTMAX_TYPE__ ssize_t;
#endif

#ifndef __LIBC_PTRDIFF_T_DEFINED__
#define __LIBC_PTRDIFF_T_DEFINED__
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif

#ifndef __LIBC_WCHAR_T_DEFINED__
#define __LIBC_WCHAR_T_DEFINED__
#define wchar_t __libc_wchar_t
typedef __WCHAR_TYPE__ wchar_t;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef offsetof
#define offsetof(t, m) \
    __builtin_offsetof(t, m)
#endif

#endif /* __INCLUDE_STDDEF_H__ */
