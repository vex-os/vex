/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __SYS_INITCALL_H__
#define __SYS_INITCALL_H__
#include <sys/cdefs.h>

#define initcall_extern(init) \
    extern void __init_##init(void)

#define initcall_binding(init, function) \
    extern void __init_##init(void) __alias(function)

#define initcall_dependency(init, dependency) \
    static const void __unused __section(".discard") (*__unique(__test)) = (&__init_##init); \
    static const void __unused __section(".discard") (*__unique(__test)) = (&__init_##dependency); \
    static const char __used __align(1) __section(".discard.initcalls") __unique(__depend)[] = (#dependency " " #init)

initcall_extern(early);
initcall_extern(common);
initcall_extern(device);

#define early_initcall(init, function) \
    initcall_binding(init, function); \
    initcall_dependency(init, early); \
    initcall_dependency(common, init)

#define common_initcall(init, function) \
    initcall_binding(init, function); \
    initcall_dependency(init, common); \
    initcall_dependency(device, init)

#define device_initcall(init, function) \
    initcall_binding(init, function); \
    initcall_dependency(init, device)

typedef struct initcall_s {
    void (*func)(void);
} initcall_t;

/* $(BUILD_DIR)/gen_initcalls.c */
extern const initcall_t initcalls[];

#endif /* __SYS_INITCALL_H__ */
