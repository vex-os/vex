#include <vex/errno.h>
#include <vex/pmap.h>
#include <vex/process.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/vms.h>

static intptr_t sys_tcb_set(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();

    if(thrd->proc == nullptr) {
        return -ENOSYS;
    }

    // A kernel address is useless to userspace and may not even be canonical
    if(args[0] >= pmap_user_end(thrd->proc->vms->pmap)) {
        return -EINVAL;
    }

    arch_thread_set_tcb(thrd, args[0]);

    return 0;
}

define_syscall(sys_tcb_set, SYS_TCB_SET);
