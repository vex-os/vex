#include <sys.arch/hart.h>
#include <sys/kprintf.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/syscall.h>

static void sys_exit(struct syscall_frame* restrict frame)
{
    struct hart_local* hart = this_hart();
    struct uthread* thread = hart->ut_curr;

    pr_inform("process %ld exited with core %d", (long)(thread->owner->id), (int)(frame->arg_0));

    uprocess_exit(thread->owner);
}

define_syscall(sys_exit, SYS_EXIT);
