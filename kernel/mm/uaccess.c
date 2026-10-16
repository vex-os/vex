#include <vex/errno.h>
#include <vex/process.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vms.h>

ssize_t user_copyin(void* restrict dst, uintptr_t src, size_t sz)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -EFAULT;
    }

    return vms_copyin(dst, proc->vms, src, sz);
}

ssize_t user_copyin_str(char* restrict dst, uintptr_t src, size_t max_sz)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -EFAULT;
    }

    return vms_copyin_str(dst, proc->vms, src, max_sz);
}

ssize_t user_copyout(uintptr_t dst, const void* restrict src, size_t sz)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -EFAULT;
    }

    return vms_copyout(proc->vms, dst, src, sz);
}

ssize_t user_copyout_str(uintptr_t dst, const char* restrict src, size_t max_sz)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -EFAULT;
    }

    return vms_copyout_str(proc->vms, dst, src, max_sz);
}
