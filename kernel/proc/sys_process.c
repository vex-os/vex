#include <vex/atomic.h>
#include <vex/binfmt.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/fcntl.h>
#include <vex/file.h>
#include <vex/kmalloc.h>
#include <vex/limits.h>
#include <vex/page.h>
#include <vex/process.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vms.h>
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

// Copies a null-terminated array of user strings, packing the strings at cursor
static int copy_strings(uintptr_t user_array, const char** restrict out, char** restrict cursor, const char* restrict limit)
{
    size_t count = 0;

    for(; user_array; count += 1) {
        uintptr_t user_string;
        ssize_t copied = user_copyin(&user_string, user_array + count * sizeof(uintptr_t), sizeof(uintptr_t));

        if(copied < 0) {
            return copied;
        }

        if(user_string == 0) {
            break;
        }

        if(count == BINFMT_ARGS_MAX || *cursor >= limit) {
            return -E2BIG;
        }

        copied = user_copyin_str(*cursor, user_string, limit - *cursor);

        if(copied == -ENAMETOOLONG) {
            return -E2BIG;
        }

        if(copied < 0) {
            return copied;
        }

        out[count] = *cursor;
        *cursor += copied + 1;
    }

    out[count] = nullptr;

    return 0;
}

static intptr_t exec_image(struct thread* restrict thrd, char* restrict path, char* restrict strings, const uintptr_t* restrict args)
{
    ssize_t copied = user_copyin_str(path, args[0], PATH_SIZE);

    if(copied < 0) {
        return copied;
    }

    const char* argv[BINFMT_ARGS_MAX + 1];
    const char* envp[BINFMT_ARGS_MAX + 1];
    char* cursor = strings;

    int retval = copy_strings(args[1], argv, &cursor, strings + PAGE_SIZE);

    if(retval == 0) {
        retval = copy_strings(args[2], envp, &cursor, strings + PAGE_SIZE);
    }

    if(retval < 0) {
        return retval;
    }

    struct vnode* stem;
    retval = syscall_dirfd(AT_FDCWD, path, &stem);

    if(retval < 0) {
        return retval;
    }

    struct process* proc = thrd->proc;

    struct file* fp;
    retval = file_open(stem, path, O_RDONLY, 0, &proc->cred, &fp);

    if(stem) {
        vnode_decref(stem);
    }

    if(retval < 0) {
        return retval;
    }

    // Built aside, so a failure leaves
    // the caller running in its old image
    struct vmspace* vms = vms_create();

    if(vms == nullptr) {
        file_close(fp);
        return -ENOMEM;
    }

    struct entry_point entry;
    retval = load_binary(vms, fp, argv, envp, &entry);

    file_close(fp);

    if(retval < 0) {
        vms_release(vms);
        return retval;
    }

    struct vmspace* old_vms = proc->vms;

    proc->vms = vms;
    arch_thread_exec(thrd, vms, entry.address, entry.ustack);

    // No hart has the old
    // page tables loaded anymore
    vms_release(old_vms);

    return 0;
}

static intptr_t sys_fork(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    struct process* child = process_create(proc);

    if(child == nullptr) {
        return -ENOMEM;
    }

    int retval = vms_clone(child->vms, proc->vms);

    if(retval == 0) {
        retval = copy_fds(child, proc);
    }

    struct thread* child_thread = nullptr;

    if(retval == 0) {
        child_thread = create_thread_fork(child, thrd->syscall_ctx);

        if(child_thread == nullptr) {
            retval = -ENOMEM;
        }
    }

    if(retval < 0) {
        process_destroy(child);
        return retval;
    }

    pid_t pid = child->pid;

    sched_enqueue(child_thread);

    return pid;
}

static intptr_t sys_exec(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();

    if(thrd->proc == nullptr) {
        return -ENOSYS;
    }

    char* path = kmalloc(PATH_SIZE);
    char* strings = pslab_alloc_virt();

    intptr_t retval = -ENOMEM;

    if(path && strings) {
        retval = exec_image(thrd, path, strings, args);
    }

    pslab_free_virt(strings);
    kfree(path);

    return retval;
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

        // A child that dies before this
        // wait starts leaves the event pending
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
