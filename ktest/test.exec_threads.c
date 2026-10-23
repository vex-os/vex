#include <vex/errno.h>
#include <vex/syscall.h>

#include "ktest.h"

static _Alignas(16) char spinner_stack[KTEST_THREAD_STACK_SIZE];
static volatile int spinner_running;
static char pid_text[24];

[[noreturn]] static void spinner(long arg)
{
    spinner_running = 1;

    for(;;) {
        // Busy on its own hart, never entering the kernel
    }
}

static long parse_number(const char* text)
{
    long number = 0;

    while(*text >= '0' && *text <= '9') {
        number = number * 10 + (*text - '0');
        text += 1;
    }

    return number;
}

static volatile unsigned int never_changes;
static const char* program_path;

// Execs from a thread that is not the main one, while one sibling sleeps in a futex
// and another spins in userspace; the exec has to take both away first
[[noreturn]] static void execer(long arg)
{
    ktest_snprintf(pid_text, sizeof(pid_text), "%ld", ktest_syscall1(__k_SYS_GETPID, 0));

    const char* argv[] = { program_path, "exec-child", pid_text, 0 };
    const char* envp[] = { 0 };

    ktest_syscall3(__k_SYS_EXEC, (long)program_path, (long)argv, (long)envp);
    ktest_syscall1(__k_SYS_EXIT, 99); // The exec failed
    __builtin_unreachable();
}

static int exec_child(char** argv)
{
    long pid = ktest_syscall1(__k_SYS_GETPID, 0);

    ktest_expect(pid == parse_number(argv[2]), 1);             // the process is the same
    ktest_expect(ktest_syscall1(__k_SYS_GETTID, 0) == pid, 1); // the surviving thread took the pid as its tid

    return 42;
}

int main(int argc, char** argv)
{
    if(argc > 2 && ktest_string_equal(argv[1], "exec-child")) {
        return exec_child(argv);
    }

    program_path = argv[0];

    // exec from a secondary thread takes the other threads away and keeps the pid
    long child = ktest_syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        ktest_syscall5(__k_SYS_THREAD_CREATE, (long)&spinner, (long)(spinner_stack + KTEST_THREAD_STACK_SIZE - 8), 0, 0, 0);

        while(!spinner_running) {
            ktest_syscall1(__k_SYS_YIELD, 0);
        }

        ktest_spawn(&execer, 0);

        for(;;) {
            ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, 0);
        }
    }

    int wstatus = -1;
    ktest_syscall2(__k_SYS_WAITPID, child, (long)&wstatus);
    ktest_expect(wstatus, 10752);

    return ktest_result();
}
