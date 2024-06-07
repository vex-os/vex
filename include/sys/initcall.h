/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INCLUDE_SYS_INITCALL_H
#define _INCLUDE_SYS_INITCALL_H
#include <sys/cdefs.h>

#define initcall(name, func) \
    extern void __init_##name(void) __alias(func)

#define initcall_extern(name) \
    extern void __init_##name(void)

#define initcall_dependency(name, depend) \
    static void __unused __section(".discard") (*__unique(__test)) = (&__init_##name); \
    static void __unused __section(".discard") (*__unique(__test)) = (&__init_##depend); \
    static const char __used __section(".initcalls") __unique(__depend)[] = (#depend " " #name)    

initcall_extern(core);
initcall_extern(subsys);
initcall_extern(device);
initcall_extern(filesystem);
initcall_extern(late);

#define core_initcall(name, func)           \
    initcall(name, func);                   \
    initcall_dependency(name, core);        \
    initcall_dependency(subsys, name)

#define subsys_initcall(name, func)         \
    initcall(name, func);                   \
    initcall_dependency(name, subsys);      \
    initcall_dependency(device, name)

#define device_initcall(name, func)         \
    initcall(name, func);                   \
    initcall_dependency(name, device);      \
    initcall_dependency(filesystem, name)

#define filesystem_initcall(name, func)     \
    initcall(name, func);                   \
    initcall_dependency(name, filesystem);  \
    initcall_dependency(late, name)

#define late_initcall(name, func)           \
    initcall(name, func);                   \
    initcall_dependency(name, late)

typedef void (*initcall_t)(void);
extern const initcall_t initcalls[];

#endif /* _INCLUDE_SYS_INITCALL_H */
