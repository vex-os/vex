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
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    const struct credentials* cred = thrd->cred;

    return copyout_res(args, cred->uid, cred->euid, cred->suid);
}

static intptr_t sys_getresgid(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    const struct credentials* cred = thrd->cred;

    return copyout_res(args, cred->gid, cred->egid, cred->sgid);
}

static intptr_t sys_setresuid(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    mutex_acquire(&proc->cred_lock);

    struct credentials* cred = cred_copy(proc->cred);
    int retval = -ENOMEM;

    if(cred) {
        retval = cred_setresuid(cred, args[0], args[1], args[2]);
    }

    if(retval == 0) {
        process_set_cred(proc, cred);
    }
    else if(cred) {
        cred_decref(cred);
    }

    mutex_release(&proc->cred_lock);

    return retval;
}

static intptr_t sys_setresgid(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    mutex_acquire(&proc->cred_lock);

    struct credentials* cred = cred_copy(proc->cred);
    int retval = -ENOMEM;

    if(cred) {
        retval = cred_setresgid(cred, args[0], args[1], args[2]);
    }

    if(retval == 0) {
        process_set_cred(proc, cred);
    }
    else if(cred) {
        cred_decref(cred);
    }

    mutex_release(&proc->cred_lock);

    return retval;
}

static intptr_t sys_getgroups(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    const struct credentials* cred = thrd->cred;
    size_t size = args[0];

    if(size == 0) {
        return cred->ngroups;
    }

    if(size < cred->ngroups) {
        return -EINVAL;
    }

    ssize_t copied = user_copyout(args[1], cred->groups, cred->ngroups * sizeof(gid_t));

    if(copied < 0) {
        return copied;
    }

    return cred->ngroups;
}

static intptr_t sys_setgroups(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    size_t size = args[0];

    if(size > atomic_load_n(&cred_ngroups_max, ATOMIC_RELAXED)) {
        return -EINVAL;
    }

    mutex_acquire(&proc->cred_lock);

    if(!cred_capable(proc->cred, CAP_SETGID)) {
        mutex_release(&proc->cred_lock);
        return -EPERM;
    }

    struct credentials* cred = cred_copy_ids(proc->cred, size);
    int retval = -ENOMEM;

    if(cred) {
        ssize_t copied = user_copyin(cred->groups, args[1], size * sizeof(gid_t));

        if(copied < 0) {
            retval = copied;
        }
        else {
            retval = cred_sort_groups(cred);
        }
    }

    if(retval == 0) {
        process_set_cred(proc, cred);
    }
    else if(cred) {
        cred_decref(cred);
    }

    mutex_release(&proc->cred_lock);

    return retval;
}

static intptr_t sys_capget(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    const struct credentials* cred = thrd->cred;

    struct k_caps caps = { 0 };
    caps.effective = cred->cap_effective;
    caps.permitted = cred->cap_permitted;
    caps.ambient = cred->cap_ambient;
    caps.bounding = cred->cap_bounding;
    caps.flags = cred->cap_flags;

    ssize_t copied = user_copyout(args[0], &caps, sizeof(caps));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

static intptr_t sys_capset(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    struct k_caps caps;
    ssize_t copied = user_copyin(&caps, args[0], sizeof(caps));

    if(copied < 0) {
        return copied;
    }

    mutex_acquire(&proc->cred_lock);

    struct credentials* cred = cred_copy(proc->cred);
    int retval = -ENOMEM;

    if(cred) {
        retval = cred_setcaps(cred, &caps);
    }

    if(retval == 0) {
        process_set_cred(proc, cred);
    }
    else if(cred) {
        cred_decref(cred);
    }

    mutex_release(&proc->cred_lock);

    return retval;
}

static intptr_t sys_getumask(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    return atomic_load_n(&proc->umask, ATOMIC_RELAXED);
}

static intptr_t sys_setumask(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    return atomic_exchange(&proc->umask, args[0] & (S_IRWXU | S_IRWXG | S_IRWXO), ATOMIC_RELAXED);
}

define_syscall(sys_getresuid, SYS_GETRESUID);
define_syscall(sys_getresgid, SYS_GETRESGID);
define_syscall(sys_setresuid, SYS_SETRESUID);
define_syscall(sys_setresgid, SYS_SETRESGID);
define_syscall(sys_getgroups, SYS_GETGROUPS);
define_syscall(sys_setgroups, SYS_SETGROUPS);
define_syscall(sys_capget, SYS_CAPGET);
define_syscall(sys_capset, SYS_CAPSET);
define_syscall(sys_getumask, SYS_GETUMASK);
define_syscall(sys_setumask, SYS_SETUMASK);
