#ifndef B2A823FB_F4AB_4DB2_9411_9E756E835A51
#define B2A823FB_F4AB_4DB2_9411_9E756E835A51
#pragma once

#include <vex/compiler.h>

#if ENABLE_ASSERTIONS
void assert_fail(const char* restrict file, unsigned long line, const char* restrict func, const char* restrict expr) __noreturn;
#endif

#if ENABLE_ASSERTIONS
#define kassert(expr)                                         \
    do {                                                      \
        if(predict_false(!(expr))) {                          \
            assert_fail(__FILE__, __LINE__, __func__, #expr); \
        }                                                     \
    } while(false)
#else
#define kassert(expr) ((void)(expr))
#endif

#endif /* B2A823FB_F4AB_4DB2_9411_9E756E835A51 */
