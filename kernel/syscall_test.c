#include <vex.arch/hart.h>
#include <vex/kprintf.h>
#include <vex/sched.h>
#include <vex/syscall.h>

// Placeholder syscalls for amd64_utest_blob (arch/amd64/kernel/utest.S),
// the smoke test exercising the syscall entry trampoline end to end. Real
// syscalls will replace these once there's an actual userland to call them.

static intptr_t sys_test(const uintptr_t* restrict args)
{
    uintptr_t argc = args[0];
    uintptr_t argv_0 = args[1];

    pr_inform("sys_test: argc=%lu argv0=\"%s\"", (unsigned long)(argc), (const char*)(argv_0));

    return 0;
}

define_syscall(sys_test, SYS_TEST);

static intptr_t sys_exit(const uintptr_t* restrict args)
{
    sched_terminate();
    unreachable();
}

define_syscall(sys_exit, SYS_EXIT);
