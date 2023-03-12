/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_INITCALL_H__
#define __INCLUDE_SYS_INITCALL_H__
#include <sys/cdefs.h>

#define __initdiscard __section(".discard")
#define __initsection __section(".discard.initcalls")

#define extern_initcall(init) \
    extern int __init_##init(void)
#define define_initcall(init, func) \
    extern int __init_##init(void) __alias(func)
#define initcall_after(init, depn) \
    static const void __unused __initdiscard (*__unique(__test)) = (&__init_##init); \
    static const void __unused __initdiscard (*__unique(__test)) = (&__init_##depn); \
    static const char __used __align(1) __initsection __unique(__depend)[] = (#depn " " #init)

extern_initcall(core);
extern_initcall(subsys);
extern_initcall(device);

#define core_initcall(init, func) \
    define_initcall(init, func); initcall_after(init, core); initcall_after(subsys, init)
#define subsys_initcall(init, func) \
    define_initcall(init, func); initcall_after(init, subsys); initcall_after(device, init)
#define device_initcall(init, func) \
    define_initcall(init, func); initcall_after(init, device);

typedef struct initcall_s {
    void (*func)(void);
} initcall_t;

// Defined in /gen/initcalls.c
extern const initcall_t initcalls[];

#endif /* __INCLUDE_SYS_INITCALL_H__ */
