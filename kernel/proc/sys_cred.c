#include <vex/atomic.h>
#include <vex/cred.h>
#include <vex/errno.h>
#include <vex/process.h>
#include <vex/stat.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>

static int copyout_res(const uintptr_t* restrict args, int32_t r, int32_t e, int32_t s)
{
    ssize_t copied = user_copyout(args[0], &r, sizeof(r));

    if(copied >= 0) {
        copied = user_copyout(args[1], &e, sizeof(e));
    }

    if(copied >= 0) {
        copied = user_copyout(args[2], &s, sizeof(s));
    }

    if(copied < 0) {
        return copied;
    }

    return 0;
}

static intptr_t sys_getresuid(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    const credentials_t* credentials = thread->credentials;

    return copyout_res(args, credentials->uid, credentials->euid, credentials->suid);
}

define_syscall(sys_getresuid, SYS_GETRESUID);

static intptr_t sys_setresuid(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    mutex_acquire(&process->credentials_lock);

    credentials_t* credentials = cred_copy(process->credentials);
    int retval = -ENOMEM;

    if(credentials) {
        retval = cred_setresuid(credentials, args[0], args[1], args[2]);
    }

    if(retval == 0) {
        process_set_cred(process, credentials);
    }
    else if(credentials) {
        cred_decref(credentials);
    }

    mutex_release(&process->credentials_lock);

    return retval;
}

define_syscall(sys_setresuid, SYS_SETRESUID);

static intptr_t sys_getresgid(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    const credentials_t* credentials = thread->credentials;

    return copyout_res(args, credentials->gid, credentials->egid, credentials->sgid);
}

define_syscall(sys_getresgid, SYS_GETRESGID);

static intptr_t sys_setresgid(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    mutex_acquire(&process->credentials_lock);

    credentials_t* credentials = cred_copy(process->credentials);
    int retval = -ENOMEM;

    if(credentials) {
        retval = cred_setresgid(credentials, args[0], args[1], args[2]);
    }

    if(retval == 0) {
        process_set_cred(process, credentials);
    }
    else if(credentials) {
        cred_decref(credentials);
    }

    mutex_release(&process->credentials_lock);

    return retval;
}

define_syscall(sys_setresgid, SYS_SETRESGID);

static intptr_t sys_getgroups(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    const credentials_t* credentials = thread->credentials;
    size_t size = args[0];

    if(size == 0) {
        return credentials->num_groups;
    }

    if(size < credentials->num_groups) {
        return -EINVAL;
    }

    ssize_t copied = user_copyout(args[1], credentials->groups, credentials->num_groups * sizeof(gid_t));

    if(copied < 0) {
        return copied;
    }

    return credentials->num_groups;
}

define_syscall(sys_getgroups, SYS_GETGROUPS);

static intptr_t sys_setgroups(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    size_t size = args[0];

    if(size > atomic_load_n(&cred_max_groups, ATOMIC_RELAXED)) {
        return -EINVAL;
    }

    mutex_acquire(&process->credentials_lock);

    if(!cred_capable(process->credentials, CAP_SETGID)) {
        mutex_release(&process->credentials_lock);
        return -EPERM;
    }

    credentials_t* credentials = cred_copy_ids(process->credentials, size);
    int retval = -ENOMEM;

    if(credentials) {
        ssize_t copied = user_copyin(credentials->groups, args[1], size * sizeof(gid_t));

        if(copied < 0) {
            retval = copied;
        }
        else {
            retval = cred_sort_groups(credentials);
        }
    }

    if(retval == 0) {
        process_set_cred(process, credentials);
    }
    else if(credentials) {
        cred_decref(credentials);
    }

    mutex_release(&process->credentials_lock);

    return retval;
}

define_syscall(sys_setgroups, SYS_SETGROUPS);

static intptr_t sys_getumask(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    return atomic_load_n(&process->umask, ATOMIC_RELAXED);
}

define_syscall(sys_getumask, SYS_GETUMASK);

static intptr_t sys_setumask(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    return atomic_exchange(&process->umask, args[0] & (S_IRWXU | S_IRWXG | S_IRWXO), ATOMIC_RELAXED);
}

define_syscall(sys_setumask, SYS_SETUMASK);

static intptr_t sys_capget(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    const credentials_t* credentials = thread->credentials;

    caps_t caps = {};
    caps.effective = credentials->cap_effective;
    caps.permitted = credentials->cap_permitted;
    caps.ambient = credentials->cap_ambient;
    caps.bounding = credentials->cap_bounding;
    caps.flags = credentials->cap_flags;

    ssize_t copied = user_copyout(args[0], &caps, sizeof(caps));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

define_syscall(sys_capget, SYS_CAPGET);

static intptr_t sys_capset(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    caps_t caps;
    ssize_t copied = user_copyin(&caps, args[0], sizeof(caps));

    if(copied < 0) {
        return copied;
    }

    mutex_acquire(&process->credentials_lock);

    credentials_t* credentials = cred_copy(process->credentials);
    int retval = -ENOMEM;

    if(credentials) {
        retval = cred_setcaps(credentials, &caps);
    }

    if(retval == 0) {
        process_set_cred(process, credentials);
    }
    else if(credentials) {
        cred_decref(credentials);
    }

    mutex_release(&process->credentials_lock);

    return retval;
}

define_syscall(sys_capset, SYS_CAPSET);
