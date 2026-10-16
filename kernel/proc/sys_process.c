#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/wait.h>

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

// Takes over the caller's reference to a claimed zombie child
static intptr_t collect_child(struct process* restrict target, uintptr_t wstatus)
{
    int status = W_EXITCODE(target->exit_code, 0);
    intptr_t retval = target->pid;

    if(wstatus) {
        ssize_t copied = user_copyout(wstatus, &status, sizeof(status));

        if(copied < 0) {
            retval = copied;
        }
    }

    process_destroy(target);
    process_decref(target);

    return retval;
}

static intptr_t wait_any_child(struct process* restrict caller, uintptr_t wstatus)
{
    for(;;) {
        struct process* target = nullptr;

        bool intr_state = spinlock_acquire_intr(&caller->lock);

        bool has_children = caller->children != nullptr;

        for(struct process* it = caller->children; it; it = it->cnext) {
            if(process_claim(it)) {
                process_incref(it);
                target = it;
                break;
            }
        }

        spinlock_release_intr(&caller->lock, intr_state);

        if(target) {
            return collect_child(target, wstatus);
        }

        if(!has_children) {
            return -ECHILD;
        }

        // A child that dies before this wait starts leaves the event pending
        struct event* child_exited = &caller->child_exited;
        ssize_t result = event_await_blocking(&child_exited, 1);

        if(result < 0) {
            return result;
        }
    }
}

static intptr_t sys_waitpid(const uintptr_t* restrict args)
{
    pid_t pid = args[0];
    uintptr_t wstatus = args[1];

    struct thread* thrd = this_thread();
    struct process* caller = thrd->proc;

    if(caller == nullptr) {
        return -ECHILD;
    }

    if(pid == -1) {
        return wait_any_child(caller, wstatus);
    }

    if(pid <= 0) {
        return -EINVAL; // Process groups don't exist yet
    }

    struct process* target = process_locate(pid);

    if(target == nullptr) {
        return -ESRCH;
    }

    if(atomic_load_n(&target->ppid, ATOMIC_RELAXED) != caller->pid) {
        process_decref(target);
        return -ECHILD;
    }

    // The event fires once, when the last thread is reaped,
    // so a process that is already a zombie has nothing to wait for
    if(!atomic_load_n(&target->zombie, ATOMIC_ACQUIRE)) {
        struct event* evnt = &target->evnt;
        ssize_t result = event_await_blocking(&evnt, 1);

        if(result < 0) {
            process_decref(target);
            return result;
        }
    }

    if(!process_claim(target)) {
        process_decref(target);
        return -ECHILD;
    }

    return collect_child(target, wstatus);
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

    return atomic_load_n(&proc->ppid, ATOMIC_RELAXED);
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
