/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_SYS_ASSERT_H
#define INCLUDE_SYS_ASSERT_H
#include <sys/panic.h>

#define kassert(x) ({if(__predict_false(!(x))){panic("assertion failed - %s", #x);UNREACHABLE();}})
#define kassertf(x, fmt, ...) ({if(__predict_false(!(x))){panic((fmt), ##__VA_ARGS__);UNREACHABLE();}})

#endif /* INCLUDE_SYS_ASSERT_H */
