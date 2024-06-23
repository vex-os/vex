/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_VEX_ASSERT_H
#define INCLUDE_VEX_ASSERT_H
#include <vex/panic.h>

#define kassert(cond) ({                        \
    if(PREDICT_FALSE(!(cond))) {                \
        panic("assertion failed: [%s]", #cond); \
        UNREACHABLE();                          \
    }})

#define kassert_msg(cond, msg) ({               \
    if(PREDICT_FALSE(!(cond))) {                \
        panic("assertion failed: [%s]", (msg)); \
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

#endif /* INCLUDE_VEX_ASSERT_H */
