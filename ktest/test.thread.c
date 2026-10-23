#include <vex/errno.h>
#include <vex/signal.h>
#include <vex/syscall.h>

#include "ktest.h"

static volatile int worker_done;
static volatile long worker_tid;
static volatile long worker_arg;
static volatile long worker_tcb_word;

[[noreturn]] static void worker(long arg)
{
    long tcb_word;
    asm volatile("movq %%fs:0, %0" : "=r"(tcb_word));

    worker_tid = ktest_syscall1(__k_SYS_GETTID, 0);
    worker_arg = arg;
    worker_tcb_word = tcb_word;
    worker_done = 1;

    ktest_syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

static char unused_stack[16];

int main(void)
{
    // A second thread gets its own tid, its argument and its TCB
    long pid = ktest_syscall1(__k_SYS_GETPID, 0);
    ktest_expect(ktest_syscall1(__k_SYS_GETTID, 0) == pid, 1);

    ktest_tls_block[0] = 0xabc;
    long tid = ktest_spawn(&worker, 42);
    ktest_expect(tid > pid, 1);

    while(!worker_done) {
        ktest_syscall1(__k_SYS_YIELD, 0);
    }

    ktest_expect(worker_tid == tid, 1);
    ktest_expect(worker_arg, 42);
    ktest_expect(worker_tcb_word, 2748);

    ktest_expect(ktest_syscall5(__k_SYS_THREAD_CREATE, -4096L, (long)(unused_stack + sizeof(unused_stack)), 0, 0, 0), -__k_EINVAL);

    return ktest_result();
}
