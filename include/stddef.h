/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STDDEF_H__
#define __INCLUDE_STDDEF_H__

#ifndef __SIZE_T_DEFINED__
#define __SIZE_T_DEFINED__ 1
typedef __SIZE_TYPE__ size_t;
#endif

#ifndef __SSIZE_T_DEFINED__
#define __SSIZE_T_DEFINED__ 1
typedef __INTMAX_TYPE__ ssize_t;
#endif

#ifndef __PTRDIFF_T_DEFINED__
#define __PTRDIFF_T_DEFINED__ 1
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
#endif

#endif /* __INCLUDE_STDDEF_H__ */
