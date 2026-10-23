#include <vex/errno.h>
#include <vex/signal.h>
#include <vex/syscall.h>

#include "ktest.h"

static volatile unsigned int never_changes;
static volatile long interrupted_result;
static volatile long handled_tid;

// Takes SIGUSR1 even when spawned by a thread that blocks it
[[noreturn]] static void sleeper(long arg)
{
    __k_sigset_t unblocked = 1UL << (__k_SIGUSR1 - 1);
    ktest_syscall3(__k_SYS_SIGPROCMASK, __k_SIG_UNBLOCK, (long)&unblocked, 0);

    interrupted_result = ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, 0);
    ktest_syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

static void on_signal(__k_int32_t sig)
{
    handled_tid = ktest_syscall1(__k_SYS_GETTID, 0);
}

int main(void)
{
    long pid = ktest_syscall1(__k_SYS_GETPID, 0);

    // futex: a signal interrupts a wait, the main thread keeps it blocked so the sleeper takes it
    struct __k_sigaction action;
    action.sa_handler = &on_signal;
    action.sa_flags = __k_SA_RESTORER;
    action.sa_restorer = &ktest_signal_restorer;
    action.sa_mask = 0;
    ktest_syscall3(__k_SYS_SIGACTION, __k_SIGUSR1, (long)&action, 0);

    ktest_spawn_joinable(&sleeper, 0);

    __k_sigset_t blocked = 1UL << (__k_SIGUSR1 - 1);
    ktest_syscall3(__k_SYS_SIGPROCMASK, __k_SIG_BLOCK, (long)&blocked, 0);

    // Sent until it lands, a signal handled before the wait began would leave the sleeper asleep
    while(ktest_join_word) {
        ktest_syscall2(__k_SYS_KILL, pid, __k_SIGUSR1);
        ktest_syscall1(__k_SYS_YIELD, 0);
    }

    ktest_expect(interrupted_result, -__k_EINTR);
    ktest_expect(handled_tid != pid, 1); // the main thread blocks the signal

    // tgkill: the signal goes to the named thread
    long sleeper_tid = ktest_spawn_joinable(&sleeper, 0);

    while(ktest_join_word) {
        ktest_syscall3(__k_SYS_TGKILL, pid, sleeper_tid, __k_SIGUSR1);
        ktest_syscall1(__k_SYS_YIELD, 0);
    }

    ktest_expect(handled_tid == sleeper_tid, 1);
    ktest_expect(ktest_syscall3(__k_SYS_TGKILL, pid, sleeper_tid, 0), -__k_ESRCH); // joined already
    ktest_expect(ktest_syscall3(__k_SYS_TGKILL, pid, pid, 0), 0);

    // One the main thread blocks stays pending on it, no other thread takes it
    ktest_syscall3(__k_SYS_TGKILL, pid, pid, __k_SIGUSR1);
    __k_sigset_t pending = 0;
    ktest_syscall1(__k_SYS_SIGPENDING, (long)&pending);
    ktest_expect(pending == blocked, 1);

    return ktest_result();
}
