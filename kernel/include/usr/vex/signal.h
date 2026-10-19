#ifndef USR_VEX_SIGNAL_H
#define USR_VEX_SIGNAL_H

#include <vex/types.h>

#define __k_SIGHUP    1
#define __k_SIGINT    2
#define __k_SIGQUIT   3
#define __k_SIGILL    4
#define __k_SIGTRAP   5
#define __k_SIGABRT   6
#define __k_SIGBUS    7
#define __k_SIGFPE    8
#define __k_SIGKILL   9
#define __k_SIGSEGV   10
#define __k_SIGPIPE   11
#define __k_SIGUSR1   12
#define __k_SIGUSR2   13
#define __k_SIGALRM   14
#define __k_SIGTERM   15
#define __k_SIGCHLD   16
#define __k_SIGCONT   17
#define __k_SIGSTOP   18
#define __k_SIGTSTP   19
#define __k_SIGTTIN   20
#define __k_SIGTTOU   21
#define __k_SIGURG    22
#define __k_SIGXCPU   23
#define __k_SIGXFSZ   24
#define __k_SIGVTALRM 25
#define __k_SIGPROF   26
#define __k_SIGWINCH  27
#define __k_SIGSYS    28
#define __k_SIGRTMIN  32
#define __k_SIGRTMAX  64
#define __k_NSIG      64

typedef __k_uint64_t __k_sigset_t;

typedef void (*__k_sighandler_t)(int);

#define __k_SIG_DFL ((__k_sighandler_t)(0))
#define __k_SIG_IGN ((__k_sighandler_t)(1))

#define __k_SA_NOCLDSTOP 0x0001
#define __k_SA_NOCLDWAIT 0x0002
#define __k_SA_SIGINFO   0x0004
#define __k_SA_ONSTACK   0x0008
#define __k_SA_RESTART   0x0010
#define __k_SA_NODEFER   0x0020
#define __k_SA_RESETHAND 0x0040
#define __k_SA_RESTORER  0x0080

#define __k_SIG_BLOCK   0
#define __k_SIG_UNBLOCK 1
#define __k_SIG_SETMASK 2

struct __k_sigaction {
    __k_sighandler_t sa_handler;
    __k_uint64_t sa_flags;
    void (*sa_restorer)(void);
    __k_sigset_t sa_mask;
};

#endif /* USR_VEX_SIGNAL_H */
