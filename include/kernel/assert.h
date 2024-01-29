/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2023, VX/sys Contributors */
#ifndef INCLUDE_KERNEL_ASSERT_H
#define INCLUDE_KERNEL_ASSERT_H
#include <kernel/panic.h>

#if defined(NDEBUG)
#define kassert(x) ((void)(x))
#define kassertf(x, fmt, ...) ((void)(x))
#else
#define kassert(x) ({if(__predict_false(!(x))){panic("assertion failed - %s", #x);__unreachable();}})
#define kassertf(x, fmt, ...) ({if(__predict_false(!(x))){panic((fmt), ##__VA_ARGS__);__unreachable();}})
#endif

#endif /* INCLUDE_KERNEL_ASSERT_H */
