/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_KAN_INITCALL_H__
#define _INCLUDE_KAN_INITCALL_H__
#include <kan/compiler.h>

typedef struct initcall_s {
    int(*func)(void);
    char name[64];
} initcall_t;

/* Defined in build/kernel/initcalls.c */
extern const initcall_t __initcalls[];

#define initcall_bind(init, func) \
    extern int __init_##init(void) __alias(func)

#define initcall_extern(init) \
    extern int __init_##init(void)

#define initcall_depend(init, dep) \
    static const void __used __section(".discard") *__concat(__test_init, __COUNTER__) = &__init_##dep; \
    static const void __used __section(".discard") *__concat(__test_init, __COUNTER__) = &__init_##init; \
    static const char __used __section(".discard.init") __aligned(1) __concat(__depend_init, __COUNTER__)[] = #dep " " #init

initcall_extern(tier_0);
initcall_extern(tier_1);
initcall_extern(tier_2);

#define initcall_tier_0(init, func) \
    initcall_bind(init, func); \
    initcall_depend(init, tier_0); \
    initcall_depend(tier_1, init)

#define initcall_tier_1(init, func) \
    initcall_bind(init, func); \
    initcall_depend(init, tier_1); \
    initcall_depend(tier_2, init)

#define initcall_tier_2(init, func) \
    initcall_bind(init, func); \
    initcall_depend(init, tier_2)

#endif /* _INCLUDE_KAN_INITCALL_H__ */
