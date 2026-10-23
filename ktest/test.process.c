#include <vex/errno.h>
#include <vex/syscall.h>

#include "ktest.h"

static volatile long waited_child;
static volatile int waited_status;
static volatile long waited_result;

static void child_waiter(long arg)
{
    int wstatus = -1;
    waited_result = ktest_syscall2(__k_SYS_WAITPID, waited_child, (long)&wstatus);
    waited_status = wstatus;
    ktest_syscall1(__k_SYS_THREAD_EXIT, 0);
}

[[noreturn]] static void finisher(long arg)
{
    // Long enough for the main thread to be gone
    for(int i = 0; i < 100; ++i) {
        ktest_syscall1(__k_SYS_YIELD, 0);
    }

    ktest_syscall1(__k_SYS_EXIT, arg);
    __builtin_unreachable();
}

int main(void)
{
    int wstatus;
    long child;

    // The process outlives its main thread, and the last thread out sets the exit code
    child = ktest_syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        ktest_spawn(&finisher, 7);
        ktest_syscall1(__k_SYS_THREAD_EXIT, 0);
    }

    wstatus = -1;
    ktest_expect(ktest_syscall2(__k_SYS_WAITPID, child, (long)&wstatus) == child, 1);
    ktest_expect(wstatus, 1792);

    child = ktest_syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        ktest_syscall1(__k_SYS_THREAD_EXIT, 0);
    }

    wstatus = -1;
    ktest_syscall2(__k_SYS_WAITPID, child, (long)&wstatus);
    ktest_expect(wstatus, 0); // a lone thread leaving is an exit with 0

    // Two threads wait for different children; the first child to die must not use up the other's wakeup
    long slow_child = ktest_syscall1(__k_SYS_FORK, 0);

    if(slow_child == 0) {
        for(int i = 0; i < 1000; ++i) {
            ktest_syscall1(__k_SYS_YIELD, 0);
        }

        ktest_syscall1(__k_SYS_EXIT, 5);
    }

    long fast_child = ktest_syscall1(__k_SYS_FORK, 0);

    if(fast_child == 0) {
        ktest_syscall1(__k_SYS_EXIT, 3);
    }

    waited_child = fast_child;
    ktest_spawn_joinable(&child_waiter, 0);

    wstatus = -1;
    ktest_expect(ktest_syscall2(__k_SYS_WAITPID, slow_child, (long)&wstatus) == slow_child, 1);
    ktest_expect(wstatus, 1280);

    ktest_join();
    ktest_expect(waited_result == fast_child, 1);
    ktest_expect(waited_status, 768);

    ktest_expect(ktest_syscall2(__k_SYS_WAITPID, slow_child, (long)&wstatus), -__k_ECHILD); // collected already
    ktest_expect(ktest_syscall2(__k_SYS_WAITPID, 1, (long)&wstatus), -__k_ECHILD);          // not a child

    return ktest_result();
}
