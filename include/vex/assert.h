#ifndef _VEX_ASSERT_H
#define _VEX_ASSERT_H 1

#include <vex/panic.h>

#define kassert(cond)                               \
    ({                                              \
        if(predict_false(!(cond))) {                \
            panic("assertion failed: [%s]", #cond); \
            unreachable();                          \
        }                                           \
    })

#endif
