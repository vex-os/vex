#include <sys.arch/hart.h>
#include <sys/kprintf.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/thread.h>

static void sys_exit(struct regfile* restrict frame)
{
    struct hart_local* hart = this_hart();
    struct thread* thread = hart->curr_thread;
    thread->owner->exit_code = (intptr_t)(frame->syscall_arg_0);
    pr_inform("sys_exit: pid=%u exit_code=%ld", thread->owner->pid, (intptr_t)(frame->syscall_arg_0));
    thread->state = THREAD_DEAD;
    thread_exit_handler(frame->syscall_arg_0);
}

define_syscall(sys_exit, SYS_EXIT);

static void sys_test(struct regfile* restrict frame)
{
    uintmax_t number = frame->syscall_number;
    uintmax_t arg_0 = frame->syscall_arg_0;
    uintmax_t arg_1 = frame->syscall_arg_1;
    uintmax_t arg_2 = frame->syscall_arg_2;
    uintmax_t arg_3 = frame->syscall_arg_3;
    uintmax_t arg_4 = frame->syscall_arg_4;
    uintmax_t arg_5 = frame->syscall_arg_5;

    pr_inform("sys_test: number=%016jX arg_0=%016jX arg_1=%016jX arg_2=%016jX", number, arg_0, arg_1, arg_2);
    pr_inform("sys_test:  arg_3=%016jX arg_4=%016jX arg_5=%016jX", arg_3, arg_4, arg_5);

    if(arg_0 != 0) {
        pr_inform("sys_test: sleeping %ju ms", arg_0);
        sched_msleep(arg_0);
        pr_inform("sys_test: woke up");
    }

    frame->syscall_retval = 0;
}

define_syscall(sys_test, SYS_TEST);

static void sys_sigreturn(struct regfile* restrict frame)
{
    struct thread* thread = this_hart()->curr_thread;
    signal_return(thread, frame);
}

define_syscall(sys_sigreturn, SYS_SIGRETURN);
