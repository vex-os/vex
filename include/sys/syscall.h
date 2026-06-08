#ifndef C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22
#define C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>
#include <vex/syscall.h>

#define SYS_EXIT __k_SYS_EXIT

struct syscall_frame {
    uintptr_t number;
    uintptr_t arg_0;
    uintptr_t arg_1;
    uintptr_t arg_2;
    uintptr_t arg_3;
    uintptr_t arg_4;
    uintptr_t arg_5;
    intptr_t retval;
} __packed;

struct syscall_metadata {
    void (*function)(struct syscall_frame* restrict frame);
    const char* identifier;
    unsigned long number;
};

#define define_syscall(sys_identifier, sys_number)                                                                 \
    static void __syscall_func_##sys_identifier(struct syscall_frame* restrict frame);                             \
    static const __used __section(".syscall_metadata") struct syscall_metadata __syscall_meta_##sys_identifier = { \
        .function = &__syscall_func_##sys_identifier,                                                              \
        .identifier = #sys_identifier,                                                                             \
        .number = (unsigned long)(sys_number),                                                                     \
    };                                                                                                             \
    static void __syscall_func_##sys_identifier(struct syscall_frame* restrict frame)

#if 0
extern struct syscall_entry syscalls_start[];
extern struct syscall_entry syscalls_end[];

void dispatch_syscall(struct syscall_frame* restrict frame);
#endif

#endif /* C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22 */
