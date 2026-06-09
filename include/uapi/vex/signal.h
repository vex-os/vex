#ifndef A0E3BC75_6B66_43DD_B9FE_E2DF84290766
#define A0E3BC75_6B66_43DD_B9FE_E2DF84290766
#pragma once

#include <vex.arch/mctx.h>

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
#define __k_SIGSYS    21
#define __k_SIGTRAP   22
#define __k_SIGURG    23
#define __k_SIGVTALRM 24
#define __k_SIGWINCH  25
#define __k_SIGXCPU   26
#define __k_SIGXFSZ   27
/* 28-31 reserved */
#define __k_SIGRTMIN 32
#define __k_SIGRTMAX 63
#define __k_NSIG     64

#define __k_SA_NOCLDSTOP ((__k_uint32_t)(0x00000001))
#define __k_SA_ONSTACK   ((__k_uint32_t)(0x00000002))
#define __k_SA_RESETHAND ((__k_uint32_t)(0x00000004))
#define __k_SA_RESTART   ((__k_uint32_t)(0x00000008))
#define __k_SA_SIGINFO   ((__k_uint32_t)(0x00000010))
#define __k_SA_NOCLDWAIT ((__k_uint32_t)(0x00000020))
#define __k_SA_NODEFER   ((__k_uint32_t)(0x00000040))

#define __k_SIG_BLOCK   0
#define __k_SIG_UNBLOCK 1
#define __k_SIG_SETMASK 2

#define __k_ILL_ILLOPC ((__k_uint32_t)(0x00000001))
#define __k_ILL_ILLOPN ((__k_uint32_t)(0x00000002))
#define __k_ILL_ILLADR ((__k_uint32_t)(0x00000003))
#define __k_ILL_ILLTRP ((__k_uint32_t)(0x00000004))
#define __k_ILL_PRVOPC ((__k_uint32_t)(0x00000005))
#define __k_ILL_PRVREG ((__k_uint32_t)(0x00000006))
#define __k_ILL_COPROC ((__k_uint32_t)(0x00000007))
#define __k_ILL_BADSTK ((__k_uint32_t)(0x00000008))

#define __k_FPE_INTDIV ((__k_uint32_t)(0x00000001))
#define __k_FPE_INTOVF ((__k_uint32_t)(0x00000002))
#define __k_FPE_FLTDIV ((__k_uint32_t)(0x00000003))
#define __k_FPE_FLTOVF ((__k_uint32_t)(0x00000004))
#define __k_FPE_FLTUND ((__k_uint32_t)(0x00000005))
#define __k_FPE_FLTRES ((__k_uint32_t)(0x00000006))
#define __k_FPE_FLTINV ((__k_uint32_t)(0x00000007))
#define __k_FPE_FLTSUB ((__k_uint32_t)(0x00000008))

#define __k_SEGV_MAPERR ((__k_uint32_t)(0x00000001))
#define __k_SEGV_ACCERR ((__k_uint32_t)(0x00000002))

#define __k_BUS_ADRALN ((__k_uint32_t)(0x00000001))
#define __k_BUS_ADRERR ((__k_uint32_t)(0x00000002))
#define __k_BUS_OBJERR ((__k_uint32_t)(0x00000003))

#define __k_TRAP_BRKPT ((__k_uint32_t)(0x00000001))
#define __k_TRAP_TRACE ((__k_uint32_t)(0x00000002))

#define __k_CLD_EXITED    ((__k_uint32_t)(0x00000001))
#define __k_CLD_KILLED    ((__k_uint32_t)(0x00000002))
#define __k_CLD_DUMPED    ((__k_uint32_t)(0x00000003))
#define __k_CLD_TRAPPED   ((__k_uint32_t)(0x00000004))
#define __k_CLD_STOPPED   ((__k_uint32_t)(0x00000005))
#define __k_CLD_CONTINUED ((__k_uint32_t)(0x00000006))

#define __k_SI_USER    ((__k_uint32_t)(0x00000000))
#define __k_SI_QUEUE   ((__k_uint32_t)(0x00000001))
#define __k_SI_TIMER   ((__k_uint32_t)(0x00000002))
#define __k_SI_ASYNCIO ((__k_uint32_t)(0x00000003))
#define __k_SI_MESGQ   ((__k_uint32_t)(0x00000004))

#define __k_SS_ONSTACK ((__k_uint32_t)(0x00000001))
#define __k_SS_DISABLE ((__k_uint32_t)(0x00000002))

#define __k_MINSIGSTKSZ 2048
#define __k_SIGSTKSZ    8192

struct __k_siginfo;

typedef void (*__k_sa_handler_t)(int);
typedef void (*__k_sa_sigaction_t)(int, struct __k_siginfo*, void*);
#define __k_SIG_DFL ((__k_sa_handler_t)(0))
#define __k_SIG_ERR ((__k_sa_handler_t)(1))
#define __k_SIG_IGN ((__k_sa_handler_t)(2))

typedef __k_uint64_t __k_sigset_t;

struct __k_stack {
    void* ss_sp;
    __k_uint64_t ss_size;
    __k_uint32_t ss_flags;
};

struct __k_ucontext {
    struct __k_ucontext* uc_link;
    struct __k_mcontext uc_mcontext;
    struct __k_stack uc_stack;
    __k_sigset_t uc_sigmask;
};

struct __k_sigaction {
    __k_sa_handler_t sa_handler;
    __k_sa_sigaction_t sa_sigaction;
    __k_uint32_t sa_flags;
    __k_sigset_t sa_mask;
};

union __k_sigval {
    __k_int32_t sival_int;
    void* sival_ptr;
};

struct __k_siginfo {
    __k_int32_t si_signo;
    __k_int32_t si_errno;
    __k_int32_t si_code;
    __k_pid_t si_pid;
    __k_uid_t si_uid;
    void* si_addr;
    __k_int32_t si_status;
    union __k_sigval si_sigval;
};

#endif /* A0E3BC75_6B66_43DD_B9FE_E2DF84290766 */
