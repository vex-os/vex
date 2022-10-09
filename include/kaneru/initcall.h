/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_INITCALL_H__
#define __INCLUDE_KANERU_INITCALL_H__
#include <kaneru/cdefs.h>

typedef int (*initcall_t)(void);

/* See /scripts/geninit.sh */
extern const initcall_t __initcalls[];

#define initcall_bind(id, func) \
    extern int __initcall_##id(void) __alias(func)

#define initcall_external(id) \
    extern int __initcall_##id(void)

/* Dependency sorting is done outside of
 * the compilation time (even after link time)
 * but we have to be sure that the initcall
 * and its dependency initcall both are declared
 * for the current translation unig (via whatever
 * macro up above: initcall_bind or initcall_extern) */
#define initcall_dependency(id, dep_id) \
    static const initcall_t __used __section(".discard.initcall.test") \
        __concat(__test_initcall, __COUNTER__) = &__initcall_##dep_id, \
        __concat(__test_initcall, __COUNTER__) = &__initcall_##id; \
    static const char __used __section(".discard.initcall.deps") __aligned(1) \
        __concat(__depend_initcall, __COUNTER__)[] = #dep_id " " #id

initcall_external(tier_0);
initcall_external(tier_1);
initcall_external(tier_2);

#define initcall_tier_0(id, func) \
    initcall_bind(id, func); \
    initcall_dependency(id, tier_0); \
    initcall_dependency(tier_1, id)

#define initcall_tier_1(id, func) \
    initcall_bind(id, func); \
    initcall_dependency(id, tier_1); \
    initcall_dependency(tier_2, id)

#define initcall_tier_2(id, func) \
    initcall_bind(id, func); \
    initcall_dependency(id, tier_2)

#endif /* __INCLUDE_KANERU_INIT_H__ */
