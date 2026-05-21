// SPDX-License-Identifier: BSD-3-Clause
#ifndef B2A823FB_F4AB_4DB2_9411_9E756E835A51
#define B2A823FB_F4AB_4DB2_9411_9E756E835A51
#pragma once

#include <core/panic.h>

#define kassert(cond)                               \
    ({                                              \
        if(predict_false(!(cond))) {                \
            panic("assertion failed: [%s]", #cond); \
            unreachable();                          \
        }                                           \
    })

#endif /* B2A823FB_F4AB_4DB2_9411_9E756E835A51 */
