/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SYS_INITCALL_H__
#define __INCLUDE_SYS_INITCALL_H__
#include <sys/cdefs.h>

typedef void (*initcall_t)(void);

/* defined in sys/kern/initcall_list.c */
/* generated via scripts/initcall_list.sh */
extern const initcall_t __initcalls[];

/* bind a function to an initcall */
#define initcall_bind(init, func) \
    extern void __init_##init(void) __alias(func)

/* forward-declare an initcall, exposing it */
#define initcall_extern(init) \
    extern void __init_##init(void)

/* set the initcall to depend on an another initcall */
#define initcall_depend(init, dep) \
    static const initcall_t __used __section(".discard") __concat(__test_init, __COUNTER__) = &__init_##dep; \
    static const initcall_t __used __section(".discard") __concat(__test_init, __COUNTER__) = &__init_##init; \
    static const char __used __section(".discard.init") __aligned(1) __concat(__depend_init, __COUNTER__)[] = #dep " " #init

/* bind a function to tier-0 initcall */
#define initcall_tier_0(init, func) \
    initcall_bind(init, func); \
    initcall_depend(init, tier_0); \
    initcall_depend(tier_1, init)

/* bind a function to tier-1 initcall */
#define initcall_tier_1(init, func) \
    initcall_bind(init, func); \
    initcall_depend(init, tier_1); \
    initcall_depend(tier_2, init)

/* bind a function to tier-2 initcall */
#define initcall_tier_2(init, func) \
    initcall_bind(init, func); \
    initcall_depend(init, tier_2)

/* defined in sys/kern/initcall.c */
initcall_extern(tier_0);
initcall_extern(tier_1);
initcall_extern(tier_2);


#endif /* __INCLUDE_SYS_INITCALL_H__ */
