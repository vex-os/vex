#include <vex/errno.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/syscall.h>
#include <vex/thread.h>

static intptr_t __noreturn sys_exit(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();

    if(thrd->proc) {
        process_exit(thrd->proc, args[0]);
    }

    sched_terminate();
    unreachable();
}

static intptr_t sys_fork(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_exec(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_waitpid(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_getpid(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_getppid(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_yield(const uintptr_t* restrict args)
{
    sched_yield();
    return 0;
}

define_syscall(sys_exit, SYS_EXIT);
define_syscall(sys_fork, SYS_FORK);
define_syscall(sys_exec, SYS_EXEC);
define_syscall(sys_waitpid, SYS_WAITPID);
define_syscall(sys_getpid, SYS_GETPID);
define_syscall(sys_getppid, SYS_GETPPID);
define_syscall(sys_yield, SYS_YIELD);
