#ifndef B2A823FB_F4AB_4DB2_9411_9E756E835A51
#define B2A823FB_F4AB_4DB2_9411_9E756E835A51
#pragma once

#include <core/cdefs.h>

extern void assertion_fail(const char* restrict file, unsigned long line, const char* restrict func, const char* restrict text) __noreturn;

#define strict_kassert(expression)                                     \
    do {                                                               \
        if(predict_false(!(expression))) {                             \
            assertion_fail(__FILE__, __LINE__, __func__, #expression); \
        }                                                              \
    } while(false)

#define strict_kassert_text(expression, text)                     \
    do {                                                          \
        if(predict_false(!(expression))) {                        \
            assertion_fail(__FILE__, __LINE__, __func__, (text)); \
        }                                                         \
    } while(false)

#ifdef PARANOID_ASSERTIONS
#define weak_kassert(expression)            strict_kassert(expression)
#define weak_kassert_text(expression, text) strict_kassert_text(expression, text)
#else
#define weak_kassert(expression)            ((void)(expression))
#define weak_kassert_text(expression, text) ((void)(expression))
#endif

#endif /* B2A823FB_F4AB_4DB2_9411_9E756E835A51 */
