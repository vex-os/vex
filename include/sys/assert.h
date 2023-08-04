/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_ASSERT_H__
#define __INCLUDE_SYS_ASSERT_H__
#include <sys/panic.h>

#if defined(NDEBUG)
#define kassert(x) ((void)(x))
#define kassertf(x, fmt, ...) ((void)(x))
#else
#define kassert(x) ({if(predict_false(!(x))){panic("assertion failed - %s", #x);unreachable();}})
#define kassertf(x, fmt, ...) ({if(predict_false(!(x))){panic((fmt), ##__VA_ARGS__);unreachable();}})
#endif

#endif /* __INCLUDE_SYS_ASSERT_H__ */
