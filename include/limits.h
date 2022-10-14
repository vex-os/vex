/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_LIMITS_H__
#define __INCLUDE_LIMITS_H__

#define CHAR_BIT __CHAR_BIT__

#define SCHAR_MIN ((signed char)(-(__SCHAR_MAX__ - 1)))
#define SCHAR_MAX ((signed char)(__SCHAR_MAX__))
#define UCHAR_MAX ((unsigned char)(-1))

#if defined(__CHAR_UNSIGNED__) || !defined(__SIGNED_CHARS__)
#define CHAR_MIN ((char)(0))
#define CHAR_MAX ((char)(UCHAR_MAX))
#else
#define CHAR_MIN ((char)(SCHAR_MIN))
#define CHAR_MAX ((char)(SCHAR_MAX))
#endif

#define SHRT_MIN ((short int)(-(__SHRT_MAX__ - 1)))
#define SHRT_MAX ((short int)(__SHRT_MAX__))
#define USHRT_MAX ((unsigned short int)(-1))

#define INT_MIN ((int)(-(__INT_MAX__ - 1)))
#define INT_MAX ((int)(__INT_MAX__))
#define UINT_MAX ((unsigned int)(-1))

#define LONG_MIN ((long int)(-(__LONG_MAX__ - 1)))
#define LONG_MAX ((long int)(__LONG_MAX__))
#define ULONG_MAX ((unsigned long int)(-1L))

#define LLONG_MIN ((long long int)(-(__LONG_LONG_MAX__ - 1)))
#define LLONG_MAX ((long long int)(__LONG_LONG_MAX__))
#define ULLONG_MAX ((unsigned long long int)(-1LL))

#endif /* __INCLUDE_LIMITS_H__ */
