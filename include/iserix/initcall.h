/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_ISERIX_INITCALL_H
#define INCLUDE_ISERIX_INITCALL_H
#include <iserix/compiler.h>

#define INITCALL(init, func) \
    extern void _init_##init(void) ALIAS(func)

#define INITCALL_EXTERN(init) \
    extern void _init_##init(void)

#define INITCALL_DEPENDENCY(init, depend) \
    static void UNUSED SECTION(".discard") (*UNIQUE(_test)) = (&_init_##init); \
    static void UNUSED SECTION(".discard") (*UNIQUE(_test)) = (&_init_##depend); \
    static const char USED SECTION(".initcalls") UNIQUE(_depend)[] = (#depend " " #init)

INITCALL_EXTERN(core);
INITCALL_EXTERN(subsys);
INITCALL_EXTERN(filesystem);
INITCALL_EXTERN(device);
INITCALL_EXTERN(late);

#define CORE_INITCALL(init, func)           \
    INITCALL(init, func);                   \
    INITCALL_DEPENDENCY(init, core);        \
    INITCALL_DEPENDENCY(subsys, init)

#define SUBSYS_INITCALL(init, func)         \
    INITCALL(init, func);                   \
    INITCALL_DEPENDENCY(init, subsys);      \
    INITCALL_DEPENDENCY(filesystem, init)

#define FILESYSTEM_INITCALL(init, func)     \
    INITCALL(init, func);                   \
    INITCALL_DEPENDENCY(init, filesystem);  \
    INITCALL_DEPENDENCY(device, init)

#define DEVICE_INITCALL(init, func)         \
    INITCALL(init, func);                   \
    INITCALL_DEPENDENCY(init, device);      \
    INITCALL_DEPENDENCY(late, init)

#define LATE_INITCALL(init, func)           \
    INITCALL(init, func);                   \
    INITCALL_DEPENDENCY(init, late)

typedef void (*initcall_t)(void);
extern const initcall_t initcalls[];

#endif /* INCLUDE_ISERIX_INITCALL_H */
