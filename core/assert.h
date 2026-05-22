// SPDX-License-Identifier: BSD-3-Clause
#ifndef B2A823FB_F4AB_4DB2_9411_9E756E835A51
#define B2A823FB_F4AB_4DB2_9411_9E756E835A51
#pragma once

#include <core/cdefs.h>

extern void assertion_fail(const char* restrict file, unsigned long line, const char* restrict func, const char* restrict expr) __noreturn;

#define kassert(expression)                                            \
    do {                                                               \
        if(predict_false(!(expression))) {                             \
            assertion_fail(__FILE__, __LINE__, __func__, #expression); \
        }                                                              \
    } while(false)

#endif /* B2A823FB_F4AB_4DB2_9411_9E756E835A51 */
