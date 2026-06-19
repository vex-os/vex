#include <vex/errno.h>
#include <vex/event.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>

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
    pid_t pid = args[0];
    uintptr_t wstatus = args[1];

    struct process* target = process_locate(pid);

    if(target == nullptr) {
        return -ESRCH;
    }

    struct thread* thrd = this_thread();
    struct process* caller = thrd->proc;

    if(caller == nullptr || target->ppid != caller->pid) {
        return -ECHILD;
    }

    struct event* evnt = &target->evnt;
    ssize_t result = event_await_blocking(&evnt, 1);

    if(result < 0) {
        return result;
    }

    int exit_code = target->exit_code;

    if(wstatus) {
        ssize_t copied = user_copyout(wstatus, &exit_code, sizeof(exit_code));

        if(copied < 0) {
            return copied;
        }
    }

    process_destroy(target);

    return pid;
}

static intptr_t sys_getpid(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ESRCH;
    }

    return proc->pid;
}

static intptr_t sys_getppid(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ESRCH;
    }

    return proc->ppid;
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
