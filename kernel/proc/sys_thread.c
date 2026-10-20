#include <vex/errno.h>
#include <vex/pmap.h>
#include <vex/process.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/vms.h>

static intptr_t sys_tcb_set(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();

    if(thread->process == nullptr) {
        return -ENOSYS;
    }

    // A kernel address is useless to userspace and may not even be canonical
    if(args[0] >= pmap_user_end(thread->process->vmspace->pmap)) {
        return -EINVAL;
    }

    arch_thread_set_tcb(thread, args[0]);

    return 0;
}

define_syscall(sys_tcb_set, SYS_TCB_SET);
