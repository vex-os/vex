#include <sys.arch/hart.h>
#include <sys/kprintf.h>
#include <sys/process.h>
#include <sys/syscall.h>
#include <sys/thread.h>

static void sys_exit(struct syscall_frame* restrict frame)
{
    struct hart_local* hart = this_hart();
    struct thread* thread = hart->curr_thread;
    thread->owner->exit_code = (intptr_t)(frame->arg_0);
    thread_exit_handler(frame->arg_0);
}

define_syscall(sys_exit, SYS_EXIT);

static void sys_test(struct syscall_frame* restrict frame)
{
    uintmax_t number = frame->number;
    uintmax_t arg_0 = frame->arg_0;
    uintmax_t arg_1 = frame->arg_1;
    uintmax_t arg_2 = frame->arg_2;
    uintmax_t arg_3 = frame->arg_3;
    uintmax_t arg_4 = frame->arg_4;
    uintmax_t arg_5 = frame->arg_5;

    pr_inform("sys_test: number=%016jX arg_0=%016jX arg_1=%016jX arg_2=%016jX", number, arg_0, arg_1, arg_2);
    pr_inform("sys_test:  arg_3=%016jX arg_4=%016jX arg_5=%016jX", arg_3, arg_4, arg_5);

    frame->retval = 0;
}

define_syscall(sys_test, SYS_TEST);
