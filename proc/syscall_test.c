#include <arch/hart.h>
#include <vex/hart.h>
#include <vex/kprintf.h>
#include <vex/mmu.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/vmmap.h>

// Placeholder syscalls for boot/utest/utest.S, the smoke test exercising
// the syscall entry trampoline and elf64 loader end to end. Real syscalls
// will replace these once there's an actual userland to call them.

#define SYS_TEST_BUFSIZE 256

static intptr_t sys_test(const uintptr_t* restrict args)
{
    uintptr_t ptr = args[0];
    size_t sz = args[1];

    if(sz >= SYS_TEST_BUFSIZE) {
        sz = SYS_TEST_BUFSIZE - 1;
    }

    char buffer[SYS_TEST_BUFSIZE];

    // FIXME: this looks horrible, TODO: possibly make something that
    // looks like Linux's copy_from_user and copy_to_user? Where do we put it?
    struct mmu_ctx* ctx = this_hart()->curr_thread->proc->map->ctx;

    size_t remaining = mmu_memcpy_from_ctx(buffer, ctx, ptr, sz);
    size_t copied = sz - remaining;

    buffer[copied] = 0;

    pr_inform("sys_test: ptr=%#lx sz=%lu data=\"%s\"", (unsigned long)(ptr), (unsigned long)(sz), buffer);

    return 0;
}

define_syscall(sys_test, SYS_TEST);

static intptr_t sys_exit(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();

    if(thrd->proc)
        process_exit(thrd->proc, args[0]);
    sched_terminate();
    unreachable();
}

define_syscall(sys_exit, SYS_EXIT);
