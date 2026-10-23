#include <vex/errno.h>
#include <vex/futex.h>
#include <vex/syscall.h>
#include <vex/time.h>

#include "ktest.h"

#define PINGPONG_ROUNDS 1000

static volatile unsigned int turn;
static volatile long pongs;
static volatile unsigned int never_changes;

[[noreturn]] static void ponger(long arg)
{
    for(long i = 0; i < arg; ++i) {
        while(turn == 0) {
            ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&turn, 0, 0, 0);
        }

        pongs += 1;
        turn = 0;
        ktest_syscall3(__k_SYS_FUTEX_WAKE, (long)&turn, 1, 0);
    }

    ktest_syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

int main(void)
{
    // futex: argument checks and timeouts
    struct __k_timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 10000000;
    ktest_expect(ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 1, 0, 0), -__k_EAGAIN);
    ktest_expect(ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, (long)&timeout, 0), -__k_ETIMEDOUT);
    ktest_expect(ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes + 1, 0, 0, 0), -__k_EINVAL);
    ktest_expect(ktest_syscall4(__k_SYS_FUTEX_WAIT, -4096L, 0, 0, 0), -__k_EFAULT);
    ktest_expect(ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, __k_FUTEX_SHARED), -__k_ENOSYS);
    ktest_expect(ktest_syscall3(__k_SYS_FUTEX_WAKE, (long)&never_changes, 1, 0), 0);
    timeout.tv_nsec = 0;
    ktest_expect(ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, (long)&timeout, 0), -__k_ETIMEDOUT);

    // futex: two threads hand a turn back and forth, then the second is joined through clear_tid
    ktest_spawn_joinable(&ponger, PINGPONG_ROUNDS);

    for(long i = 0; i < PINGPONG_ROUNDS; ++i) {
        turn = 1;
        ktest_syscall3(__k_SYS_FUTEX_WAKE, (long)&turn, 1, 0);

        while(turn == 1) {
            ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&turn, 1, 0, 0);
        }
    }

    ktest_join();
    ktest_expect(pongs, 1000);
    ktest_expect(ktest_join_word, 0);

    return ktest_result();
}
