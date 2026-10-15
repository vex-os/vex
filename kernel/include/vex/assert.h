#pragma once

#include <vex/compiler.h>

#ifdef CONFIG_KASSERT
void assert_fail(const char* restrict file, unsigned long line, const char* restrict func, const char* restrict expr) __noreturn;
#endif

#ifdef CONFIG_KASSERT
#define kassert(expr)                                         \
    do {                                                      \
        if(predict_false(!(expr))) {                          \
            assert_fail(__FILE__, __LINE__, __func__, #expr); \
        }                                                     \
    } while(false)
#else
#define kassert(expr) ((void)(expr))
#endif
