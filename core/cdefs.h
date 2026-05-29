#ifndef A3F007DC_A45C_452A_9065_57ADF398B430
#define A3F007DC_A45C_452A_9065_57ADF398B430
#pragma once

#include <vex/cdefs.h>

#if __has_builtin(__builtin_expect)
#define predict_false(cond) __builtin_expect((cond), 0L)
#define predict_true(cond)  __builtin_expect((cond), 1L)
#else
#define predict_false(cond) (cond)
#define predict_true(cond)  (cond)
#endif

#if __has_builtin(__builtin_unreachable)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() \
    do {              \
    } while(1)
#endif

#define UNUSED_ARG(arg) ((void)(arg))

#define CONCAT0(x, y) x##y
#define CONCAT(x, y)  CONCAT0(x, y)
#define UNIQUE(x)     CONCAT(x, __COUNTER__)

#define STRINGIFY0(x)  #x
#define STRINGIFY(x)   STRINGIFY0(x)
#define STRING_VA(...) #__VA_ARGS__

#define align_ceil(x, align)  (((x) + (__typeof__(x))((align) - 1)) & ~(__typeof__(x))((align) - 1))
#define align_floor(x, align) ((x) & ~(__typeof__(x))((align) - 1))

#endif /* A3F007DC_A45C_452A_9065_57ADF398B430 */
