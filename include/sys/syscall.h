#ifndef C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22
#define C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22
#pragma once

#include <sys.arch/regs.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <vex/syscall.h>

#define SYS_EXIT      __k_SYS_EXIT
#define SYS_SLEEP     __k_SYS_SLEEP
#define SYS_SIGRETURN __k_SYS_SIGRETURN

#define SYS_TEST 42

struct syscall {
    void (*function)(struct regfile* restrict frame);
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

void dispatch_syscall(struct regfile* restrict frame);

#endif /* C2F4E5EB_E2C3_4357_ADD8_6EAE11DBCB22 */
