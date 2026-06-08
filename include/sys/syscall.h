#ifndef C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22
#define C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>
#include <vex/syscall.h>

#define SYS_EXIT  __k_SYS_EXIT
#define SYS_SLEEP __k_SYS_SLEEP

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

struct syscall {
    void (*function)(struct syscall_frame* restrict frame);
    const char* identifier;
    unsigned long number;
};

#define define_syscall(sc_function, sc_number)                                            \
    static const __used __section(".syscalls") struct syscall __syscall_##sc_function = { \
        .function = &(sc_function),                                                       \
        .identifier = #sc_function,                                                       \
        .number = (sc_number),                                                            \
    }

extern const struct syscall unsorted_syscalls_start[];
extern const struct syscall unsorted_syscalls_end[];

void dispatch_syscall(struct syscall_frame* restrict frame);

#endif /* C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22 */
