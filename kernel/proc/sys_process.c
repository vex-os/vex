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
#include <vex/signal.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vms.h>
#include <vex/wait.h>

static int copyin_string_array(uintptr_t user_array, const char** restrict out, char** restrict cursor, const char* restrict limit)
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

static int read_file_caps(vnode_t* restrict node, file_caps_t* restrict out, bool* restrict has_caps)
{
    vfs_cap_data_t data;
    ssize_t size = vnode_caps_get(node, &data, sizeof(data));

    *has_caps = false;

    if(size == -ENODATA || size == -EOPNOTSUPP) {
        return 0;
    }

    // Larger than any revision
    if(size == -ERANGE) {
        return -EINVAL;
    }

    if(size < 0) {
        return size;
    }

    int retval = cred_parse_file_caps(&data, size, out);

    if(retval == -ENODATA) {
        return 0;
    }

    if(retval < 0) {
        return retval;
    }

    *has_caps = true;

    return 0;
}

static int exec_cred(const credentials_t* restrict credentials, const stat_t* restrict st, const file_caps_t* restrict file,
    credentials_t** restrict out)
{
    credentials_t* copy = cred_copy(credentials);

    if(copy == nullptr) {
        return -ENOMEM;
    }

    bool setuid = st->st_mode & S_ISUID;
    bool setgid = (st->st_mode & S_ISGID) && (st->st_mode & S_IXGRP);

    if(setuid) {
        copy->euid = st->st_uid;
    }

    if(setgid) {
        copy->egid = st->st_gid;
    }

    copy->suid = copy->euid;
    copy->sgid = copy->egid;

    int retval = cred_exec_caps(copy, setuid || setgid, file);

    if(retval < 0) {
        cred_decref(copy);
        return retval;
    }

    *out = copy;

    return 0;
}

static intptr_t exec_image(thread_t* restrict thread, char* restrict path, char* restrict strings, const uintptr_t* restrict args)
{
    ssize_t copied = user_copyin_str(path, args[0], PATH_SIZE);

    if(copied < 0) {
        return copied;
    }

    const char* argv[BINFMT_ARGS_MAX + 1];
    const char* envp[BINFMT_ARGS_MAX + 1];
    char* cursor = strings;

    int retval = copyin_string_array(args[1], argv, &cursor, strings + PAGE_SIZE);

    if(retval == 0) {
        retval = copyin_string_array(args[2], envp, &cursor, strings + PAGE_SIZE);
    }

    if(retval < 0) {
        return retval;
    }

    process_t* process = thread->process;
    const credentials_t* credentials = thread->credentials;

    vnode_t* node;
    retval = syscall_resolve_at(AT_FDCWD, path, 0, &node);

    if(retval < 0) {
        return retval;
    }

    if(node->type != VNODE_FILE) {
        vnode_decref(node);
        return -EACCES;
    }

    stat_t st;
    retval = vnode_access(node, credentials, X_OK);

    if(retval == 0) {
        retval = vnode_stat(node, &st);
    }

    file_caps_t file_caps;
    bool has_caps;

    if(retval == 0) {
        retval = read_file_caps(node, &file_caps, &has_caps);
    }

    if(retval < 0) {
        vnode_decref(node);
        return retval;
    }

    file_t* file;
    retval = file_open_node(node, O_RDONLY, credentials, &file);

    if(retval < 0) {
        return retval;
    }

    // Built aside, so a failure leaves
    // the caller running in its old image
    vmspace_t* vmspace = vms_create();

    if(vmspace == nullptr) {
        file_close(file);
        return -ENOMEM;
    }

    mutex_acquire(&process->credentials_lock);

    credentials_t* new_cred = nullptr;
    binfmt_entry_t entry;

    if(has_caps) {
        retval = exec_cred(process->credentials, &st, &file_caps, &new_cred);
    }
    else {
        retval = exec_cred(process->credentials, &st, nullptr, &new_cred);
    }

    if(retval == 0) {
        retval = load_binary(vmspace, file, argv, envp, new_cred, &entry);
    }

    mutex_release(&process->credentials_lock);

    file_close(file);

    // The other threads go only once the new image is in place, and not under the
    // credentials lock: one of them may be waiting for it and never get to exit
    if(retval == 0) {
        retval = process_dethread(process, thread);
    }

    if(retval < 0) {
        if(new_cred) {
            cred_decref(new_cred);
        }

        vms_release(vmspace);
        return retval;
    }

    mutex_acquire(&process->credentials_lock);
    process_set_cred(process, new_cred);
    mutex_release(&process->credentials_lock);

    vmspace_t* old_vms = process->vmspace;

    process->vmspace = vmspace;
    arch_thread_exec(thread, vmspace, entry.address, entry.user_stack);
    signal_actions_exec(process->signal_actions);
    thread->sig_altstack = 0;
    thread->sig_altstack_size = 0;
    thread->clear_tid = 0; // An address in the image just left

    vms_release(old_vms);

    return 0;
}

static intptr_t collect_child(process_t* restrict target, uintptr_t wstatus)
{
    int status = W_EXITCODE(target->exit_code, target->exit_signal);
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

static intptr_t wait_any_child(process_t* restrict caller, uintptr_t wstatus)
{
    for(;;) {
        process_t* target = nullptr;

        bool intr_state = spinlock_acquire_intr(&caller->lock);

        bool has_children = caller->children != nullptr;

        for(process_t* it = caller->children; it; it = it->sibling_next) {
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
        event_t* child_exited = &caller->child_exited;
        ssize_t result = event_await_interruptible(&child_exited, 1);

        if(result < 0) {
            return result;
        }
    }
}

static intptr_t __noreturn sys_exit(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();

    if(thread->process) {
        process_exit(thread->process, args[0]);
    }

    sched_terminate();
    unreachable();
}

static intptr_t sys_fork(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    process_t* child = process_create(process);

    if(child == nullptr) {
        return -ENOMEM;
    }

    int retval = vms_clone(child->vmspace, process->vmspace);

    if(retval == 0) {
        retval = process_fds_copy(child, process);
    }

    thread_t* child_thread = nullptr;

    if(retval == 0) {
        child_thread = thread_create_fork(child, thread->syscall_ctx);

        if(child_thread == nullptr) {
            retval = -ENOMEM;
        }
    }

    if(retval < 0) {
        process_destroy(child);
        return retval;
    }

    pid_t pid = child->pid;

    child_thread->sig_blocked = thread->sig_blocked;
    child_thread->sig_altstack = thread->sig_altstack;
    child_thread->sig_altstack_size = thread->sig_altstack_size;

    sched_enqueue(child_thread);

    return pid;
}

static intptr_t sys_exec(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();

    if(thread->process == nullptr) {
        return -ENOSYS;
    }

    char* path = kmalloc(PATH_SIZE);
    char* strings = pslab_alloc_virt();

    intptr_t retval = -ENOMEM;

    if(path && strings) {
        retval = exec_image(thread, path, strings, args);
    }

    pslab_free_virt(strings);
    kfree(path);

    return retval;
}

static intptr_t sys_waitpid(const uintptr_t* restrict args)
{
    pid_t pid = args[0];
    uintptr_t wstatus = args[1];

    thread_t* thread = this_thread();
    process_t* caller = thread->process;

    if(caller == nullptr) {
        return -ECHILD;
    }

    if(pid == -1) {
        return wait_any_child(caller, wstatus);
    }

    if(pid <= 0) {
        return -EINVAL; // Process groups don't exist yet
    }

    process_t* target = process_locate(pid);

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
        event_t* event = &target->event;
        ssize_t result = event_await_interruptible(&event, 1);

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
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ESRCH;
    }

    return process->pid;
}

static intptr_t sys_getppid(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ESRCH;
    }

    return atomic_load_n(&process->ppid, ATOMIC_RELAXED);
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
