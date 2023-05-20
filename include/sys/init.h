/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_INIT_H__
#define __INCLUDE_SYS_INIT_H__
#include <sys/cdefs.h>

#define initcall(init, func) \
    extern void __init_##init(void) __alias(func)

#define initcall_extern(init) \
    extern void __init_##init(void)

#define initcall_depend(init, depn) \
    static const void __unused __section(".discard") (*__unique(__test)) = (&__init_##depn); \
    static const void __unused __section(".discard") (*__unique(__test)) = (&__init_##init); \
    static const char __used __section(".discard.initcalls") __unique(__depend)[] = (#depn " " #init)

initcall_extern(core);
initcall_extern(subsys);
initcall_extern(filesystem);
initcall_extern(device);
initcall_extern(late);

#define core_initcall(init, func) initcall(init, func); initcall_depend(init, core); initcall_depend(subsys, init)
#define subsys_initcall(init, func) initcall(init, func); initcall_depend(init, subsys); initcall_depend(filesystem, init)
#define filesystem_initcall(init, func) initcall(init, func); initcall_depend(init, filesystem); initcall_depend(device, init)
#define device_initcall(init, func) initcall(init, func); initcall_depend(init, device); initcall_depend(late, init)
#define late_initcall(init, func) initcall(init, func); initcall_depend(init, late)

typedef void (*initcall_t)(void);
extern const initcall_t initcalls[];

#endif /* __INCLUDE_SYS_INIT_H__ */
