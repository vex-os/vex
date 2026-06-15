#ifndef DB28D5EE_DEE5_4E22_9DD6_78A1F88FF744
#define DB28D5EE_DEE5_4E22_9DD6_78A1F88FF744
#pragma once

#include <usr/vex/syscall.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define SYS_EXIT __k_SYS_EXIT
#define SYS_TEST __k_SYS_TEST

struct hart_context;

#define SYS_EXIT __k_SYS_EXIT
#define SYS_TEST __k_SYS_TEST

struct syscall {
    void (*function)(struct hart_context* restrict regs);
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

void dispatch_syscall(struct hart_context* restrict regs);
#endif /* DB28D5EE_DEE5_4E22_9DD6_78A1F88FF744 */
