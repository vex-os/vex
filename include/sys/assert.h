/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_ASSERT_H
#define _INCLUDE_SYS_ASSERT_H
#include <sys/panic.h>

#define kassert(cond) ({                        \
    if(PREDICT_FALSE(!(cond))) {                \
        panic("assertion failed: %s", #cond);   \
        UNREACHABLE();                          \
    }})

#define kassert_msg(cond, msg) ({               \
    if(PREDICT_FALSE(!(cond))) {                \
        panic("assertion failed: %s", (msg));   \
        UNREACHABLE();                          \
    }})

#define kassert_fmt(cond, fmt, ...) ({          \
    if(PREDICT_FALSE(!(cond))) {                \
        panic((fmt), ##__VA_ARGS__);            \
        UNREACHABLE();                          \
    }})

#define kassert_vfmt(cond, fmt, ap) ({          \
    if(PREDICT_FALSE(!(cond))) {                \
        vpanic((fmt), (ap));                    \
        UNREACHABLE();                          \
    }})

#endif /* _INCLUDE_SYS_ASSERT_H */
