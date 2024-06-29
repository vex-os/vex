// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_KERN_ASSERT_H
#define INCLUDE_KERN_ASSERT_H
#include <kern/panic.h>

#define kassert(cond) ({                        \
    if(predict_false(!(cond))) {                \
        panic("assertion failed: [%s]", #cond); \
        unreachable();                          \
    }})

#define kassert_msg(cond, msg) ({               \
    if(predict_false(!(cond))) {                \
        panic("assertion failed: [%s]", (msg)); \
        unreachable();                          \
    }})

#define kassert_fmt(cond, fmt, ...) ({          \
    if(predict_false(!(cond))) {                \
        panic((fmt), ##__VA_ARGS__);            \
        unreachable();                          \
    }})

#define kassert_vfmt(cond, fmt, ap) ({          \
    if(predict_false(!(cond))) {                \
        vpanic((fmt), (ap));                    \
        unreachable();                          \
    }})

#endif // INCLUDE_KERN_ASSERT_H
