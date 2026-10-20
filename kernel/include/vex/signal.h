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

#define SI_USER       __k_SI_USER
#define SI_QUEUE      __k_SI_QUEUE
#define SI_TIMER      __k_SI_TIMER
#define SI_MESGQ      __k_SI_MESGQ
#define SI_ASYNCIO    __k_SI_ASYNCIO
#define SI_KERNEL     __k_SI_KERNEL
#define ILL_ILLOPC    __k_ILL_ILLOPC
#define ILL_ILLOPN    __k_ILL_ILLOPN
#define ILL_ILLADR    __k_ILL_ILLADR
#define ILL_ILLTRP    __k_ILL_ILLTRP
#define ILL_PRVOPC    __k_ILL_PRVOPC
#define ILL_PRVREG    __k_ILL_PRVREG
#define ILL_COPROC    __k_ILL_COPROC
#define ILL_BADSTK    __k_ILL_BADSTK
#define FPE_INTDIV    __k_FPE_INTDIV
#define FPE_INTOVF    __k_FPE_INTOVF
#define FPE_FLTDIV    __k_FPE_FLTDIV
#define FPE_FLTOVF    __k_FPE_FLTOVF
#define FPE_FLTUND    __k_FPE_FLTUND
#define FPE_FLTRES    __k_FPE_FLTRES
#define FPE_FLTINV    __k_FPE_FLTINV
#define FPE_FLTSUB    __k_FPE_FLTSUB
#define SEGV_MAPERR   __k_SEGV_MAPERR
#define SEGV_ACCERR   __k_SEGV_ACCERR
#define BUS_ADRALN    __k_BUS_ADRALN
#define BUS_ADRERR    __k_BUS_ADRERR
#define BUS_OBJERR    __k_BUS_OBJERR
#define TRAP_BRKPT    __k_TRAP_BRKPT
#define TRAP_TRACE    __k_TRAP_TRACE
#define CLD_EXITED    __k_CLD_EXITED
#define CLD_KILLED    __k_CLD_KILLED
#define CLD_DUMPED    __k_CLD_DUMPED
#define CLD_TRAPPED   __k_CLD_TRAPPED
#define CLD_STOPPED   __k_CLD_STOPPED
#define CLD_CONTINUED __k_CLD_CONTINUED

#define k_sigaction __k_sigaction
#define k_siginfo   __k_siginfo
#define k_stack     __k_stack
#define k_ucontext  __k_ucontext

#define SS_ONSTACK __k_SS_ONSTACK
#define SS_DISABLE __k_SS_DISABLE
#define UC_FPU     __k_UC_FPU

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

struct sigqueue {
    struct sigqueue* next;
    struct k_siginfo info;
};

struct hart_context;
struct process;
struct thread;

struct sighand* sighand_create(struct sighand* restrict parent) __nodiscard;
void sighand_destroy(struct sighand* restrict sighand);
void sighand_exec(struct sighand* restrict sighand);

int signal_send(struct process* restrict proc, struct thread* restrict target, const struct k_siginfo* restrict info);
void signal_send_node(struct process* restrict proc, struct sigqueue* restrict node);
void signal_force(struct thread* restrict thrd, const struct k_siginfo* restrict info);
void signal_deliver(struct hart_context* restrict ctx);
void signal_action(struct process* restrict proc, int sig, const struct k_sigaction* restrict act, struct k_sigaction* restrict oact);
sigset_t signal_mask(struct thread* restrict thrd, int how, sigset_t set);
sigset_t signal_pending(struct thread* restrict thrd);
void signal_flush(struct sigqueue* restrict queue);
bool signal_on_altstack(const struct thread* restrict thrd, uintptr_t sp) __nodiscard;
void signal_altstack_get(const struct thread* restrict thrd, uintptr_t sp, struct k_stack* restrict out);

int arch_signal_setup(struct thread* restrict thrd, struct hart_context* restrict ctx, const struct k_sigaction* restrict action,
    const struct k_siginfo* restrict info, sigset_t old_mask);
int arch_signal_return(struct thread* restrict thrd, struct hart_context* restrict ctx, sigset_t* restrict mask);
void arch_syscall_interrupted(struct hart_context* restrict ctx);
