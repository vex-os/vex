#ifndef A0E3BC75_6B66_43DD_B9FE_E2DF84290766
#define A0E3BC75_6B66_43DD_B9FE_E2DF84290766
#pragma once

#include <vex/types.h>

#define __k_SIGABRT   1
#define __k_SIGALRM   2
#define __k_SIGBUS    3
#define __k_SIGCHLD   4
#define __k_SIGCONT   5
#define __k_SIGFPE    6
#define __k_SIGHUP    7
#define __k_SIGILL    8
#define __k_SIGINT    9
#define __k_SIGKILL   10
#define __k_SIGPIPE   11
#define __k_SIGQUIT   12
#define __k_SIGSEGV   13
#define __k_SIGSTOP   14
#define __k_SIGTERM   15
#define __k_SIGTSTP   16
#define __k_SIGTTIN   17
#define __k_SIGTTOU   18
#define __k_SIGUSR1   19
#define __k_SIGUSR2   20
#define __k_SIGPOLL   21
#define __k_SIGPROF   22
#define __k_SIGSYS    23
#define __k_SIGTRAP   24
#define __k_SIGURG    25
#define __k_SIGVTALRM 26
#define __k_SIGXCPU   27
#define __k_SIGXFSZ   28

#define __k_SIGRTMIN 32
#define __k_SIGRTMAX 63

#define __k_NSIG 64

typedef void (*__k_sighandler_t)(int);
#define __k_SIG_DFL ((__k_sighandler_t)(0))
#define __k_SIG_ERR ((__k_sighandler_t)(1))
#define __k_SIG_IGN ((__k_sighandler_t)(2))

typedef __k_uint64_t __k_sigset_t;

#define __k_SA_NOCLDSTOP ((__k_uint32_t)(0x00000001))
#define __k_SA_NOCLDWAIT ((__k_uint32_t)(0x00000002))
#define __k_SA_SIGINFO   ((__k_uint32_t)(0x00000004))
#define __k_SA_ONSTACK   ((__k_uint32_t)(0x00000008))
#define __k_SA_RESTART   ((__k_uint32_t)(0x00000010))
#define __k_SA_NODEFER   ((__k_uint32_t)(0x00000020))
#define __k_SA_RESETHAND ((__k_uint32_t)(0x00000040))

struct __k_sigaction {
    __k_sighandler_t sa_handler;
    __k_sigset_t sa_mask;
    __k_uint32_t sa_flags;
};

#endif /* A0E3BC75_6B66_43DD_B9FE_E2DF84290766 */
