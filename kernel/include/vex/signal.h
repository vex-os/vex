#pragma once

#include <usr/vex/signal.h>
#include <vex/compiler.h>
#include <vex/spinlock.h>

#define SIGHUP    __k_SIGHUP
#define SIGINT    __k_SIGINT
#define SIGQUIT   __k_SIGQUIT
#define SIGILL    __k_SIGILL
#define SIGTRAP   __k_SIGTRAP
#define SIGABRT   __k_SIGABRT
#define SIGBUS    __k_SIGBUS
#define SIGFPE    __k_SIGFPE
#define SIGKILL   __k_SIGKILL
#define SIGSEGV   __k_SIGSEGV
#define SIGPIPE   __k_SIGPIPE
#define SIGUSR1   __k_SIGUSR1
#define SIGUSR2   __k_SIGUSR2
#define SIGALRM   __k_SIGALRM
#define SIGTERM   __k_SIGTERM
#define SIGCHLD   __k_SIGCHLD
#define SIGCONT   __k_SIGCONT
#define SIGSTOP   __k_SIGSTOP
#define SIGTSTP   __k_SIGTSTP
#define SIGTTIN   __k_SIGTTIN
#define SIGTTOU   __k_SIGTTOU
#define SIGURG    __k_SIGURG
#define SIGXCPU   __k_SIGXCPU
#define SIGXFSZ   __k_SIGXFSZ
#define SIGVTALRM __k_SIGVTALRM
#define SIGPROF   __k_SIGPROF
#define SIGWINCH  __k_SIGWINCH
#define SIGSYS    __k_SIGSYS
#define SIGRTMIN  __k_SIGRTMIN
#define SIGRTMAX  __k_SIGRTMAX
#define NSIG      __k_NSIG

#define SIG_DFL      __k_SIG_DFL
#define SIG_IGN      __k_SIG_IGN
#define SA_NOCLDSTOP __k_SA_NOCLDSTOP
#define SA_NOCLDWAIT __k_SA_NOCLDWAIT
#define SA_SIGINFO   __k_SA_SIGINFO
#define SA_ONSTACK   __k_SA_ONSTACK
#define SA_RESTART   __k_SA_RESTART
#define SA_NODEFER   __k_SA_NODEFER
#define SA_RESETHAND __k_SA_RESETHAND
#define SA_RESTORER  __k_SA_RESTORER
#define SIG_BLOCK    __k_SIG_BLOCK
#define SIG_UNBLOCK  __k_SIG_UNBLOCK
#define SIG_SETMASK  __k_SIG_SETMASK

#define k_sigaction __k_sigaction

typedef __k_sigset_t sigset_t;
typedef __k_sighandler_t sighandler_t;

static __always_inline __nodiscard sigset_t sigbit(int sig)
{
    return (sigset_t)(1) << (sig - 1);
}

struct sighand {
    spinlock_type lock;
    struct k_sigaction actions[NSIG];
};

struct sighand* sighand_create(struct sighand* restrict parent) __nodiscard;
void sighand_destroy(struct sighand* restrict sighand);
void sighand_exec(struct sighand* restrict sighand);
