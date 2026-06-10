#ifndef E9456F4D_FA9E_4E06_9DA4_B0D7765C0CAC
#define E9456F4D_FA9E_4E06_9DA4_B0D7765C0CAC
#pragma once

#include <usr/vex/signal.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define SIGABRT   __k_SIGABRT
#define SIGALRM   __k_SIGALRM
#define SIGBUS    __k_SIGBUS
#define SIGCHLD   __k_SIGCHLD
#define SIGCONT   __k_SIGCONT
#define SIGFPE    __k_SIGFPE
#define SIGHUP    __k_SIGHUP
#define SIGILL    __k_SIGILL
#define SIGINT    __k_SIGINT
#define SIGKILL   __k_SIGKILL
#define SIGPIPE   __k_SIGPIPE
#define SIGQUIT   __k_SIGQUIT
#define SIGSEGV   __k_SIGSEGV
#define SIGSTOP   __k_SIGSTOP
#define SIGTERM   __k_SIGTERM
#define SIGTSTP   __k_SIGTSTP
#define SIGTTIN   __k_SIGTTIN
#define SIGTTOU   __k_SIGTTOU
#define SIGUSR1   __k_SIGUSR1
#define SIGUSR2   __k_SIGUSR2
#define SIGSYS    __k_SIGSYS
#define SIGTRAP   __k_SIGTRAP
#define SIGURG    __k_SIGURG
#define SIGVTALRM __k_SIGVTALRM
#define SIGWINCH  __k_SIGWINCH
#define SIGXCPU   __k_SIGXCPU
#define SIGXFSZ   __k_SIGXFSZ
#define SIGRTMIN  __k_SIGRTMIN
#define SIGRTMAX  __k_SIGRTMAX
#define NSIG      __k_NSIG

#define SA_NOCLDSTOP __k_SA_NOCLDSTOP
#define SA_ONSTACK   __k_SA_ONSTACK
#define SA_RESETHAND __k_SA_RESETHAND
#define SA_RESTART   __k_SA_RESTART
#define SA_SIGINFO   __k_SA_SIGINFO
#define SA_NOCLDWAIT __k_SA_NOCLDWAIT
#define SA_NODEFER   __k_SA_NODEFER

#define SIG_BLOCK   __k_SIG_BLOCK
#define SIG_UNBLOCK __k_SIG_UNBLOCK
#define SIG_SETMASK __k_SIG_SETMASK

#define ILL_ILLOPC __k_ILL_ILLOPC
#define ILL_ILLOPN __k_ILL_ILLOPN
#define ILL_ILLADR __k_ILL_ILLADR
#define ILL_ILLTRP __k_ILL_ILLTRP
#define ILL_PRVOPC __k_ILL_PRVOPC
#define ILL_PRVREG __k_ILL_PRVREG
#define ILL_COPROC __k_ILL_COPROC
#define ILL_BADSTK __k_ILL_BADSTK

#define FPE_INTDIV __k_FPE_INTDIV
#define FPE_INTOVF __k_FPE_INTOVF
#define FPE_FLTDIV __k_FPE_FLTDIV
#define FPE_FLTOVF __k_FPE_FLTOVF
#define FPE_FLTUND __k_FPE_FLTUND
#define FPE_FLTRES __k_FPE_FLTRES
#define FPE_FLTINV __k_FPE_FLTINV
#define FPE_FLTSUB __k_FPE_FLTSUB

#define SEGV_MAPERR __k_SEGV_MAPERR
#define SEGV_ACCERR __k_SEGV_ACCERR

#define BUS_ADRALN __k_BUS_ADRALN
#define BUS_ADRERR __k_BUS_ADRERR
#define BUS_OBJERR __k_BUS_OBJERR

#define TRAP_BRKPT __k_TRAP_BRKPT
#define TRAP_TRACE __k_TRAP_TRACE

#define CLD_EXITED    __k_CLD_EXITED
#define CLD_KILLED    __k_CLD_KILLED
#define CLD_DUMPED    __k_CLD_DUMPED
#define CLD_TRAPPED   __k_CLD_TRAPPED
#define CLD_STOPPED   __k_CLD_STOPPED
#define CLD_CONTINUED __k_CLD_CONTINUED

#define SI_USER    __k_SI_USER
#define SI_QUEUE   __k_SI_QUEUE
#define SI_TIMER   __k_SI_TIMER
#define SI_ASYNCIO __k_SI_ASYNCIO
#define SI_MESGQ   __k_SI_MESGQ

#define SS_ONSTACK __k_SS_ONSTACK
#define SS_DISABLE __k_SS_DISABLE

#define MINSIGSTKSZ __k_MINSIGSTKSZ
#define SIGSTKSZ    __k_SIGSTKSZ

#define SIG_DFL __k_SIG_DFL
#define SIG_ERR __k_SIG_ERR
#define SIG_IGN __k_SIG_IGN

#define SIGRETURN_GUARD_VIRT 0x1000UL

typedef __k_sa_handler_t sa_handler_t;
typedef __k_sa_sigaction_t sa_sigaction_t;
typedef __k_sigset_t sigset_t;

typedef struct __k_stack stack_t;
#define k_ucontext  __k_ucontext
#define k_sigaction __k_sigaction
#define k_sigval    __k_sigval
#define k_siginfo   __k_siginfo

struct process;
struct regfile;
struct thread;

bool signal_pending(struct thread* restrict thrd);
void signal_deliver_base(struct thread* restrict thrd, struct regfile* restrict regs, const struct k_siginfo* restrict siginfo_src);
void signal_deliver_sync(struct thread* restrict thrd, struct regfile* restrict regs, const struct k_siginfo* restrict siginfo_src);
void signal_deliver_auto(struct thread* restrict thrd, struct regfile* restrict regs);

void signal_psend(struct process* restrict proc, int signum);
void signal_tsend(struct thread* restrict thrd, int signum);

void signal_return(struct thread* restrict thrd, struct regfile* restrict regs);

#endif /* E9456F4D_FA9E_4E06_9DA4_B0D7765C0CAC */
