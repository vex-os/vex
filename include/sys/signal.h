#ifndef E9456F4D_FA9E_4E06_9DA4_B0D7765C0CAC
#define E9456F4D_FA9E_4E06_9DA4_B0D7765C0CAC
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>
#include <vex/signal.h>

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
#define SIGPOLL   __k_SIGPOLL
#define SIGPROF   __k_SIGPROF
#define SIGSYS    __k_SIGSYS
#define SIGTRAP   __k_SIGTRAP
#define SIGURG    __k_SIGURG
#define SIGVTALRM __k_SIGVTALRM
#define SIGXCPU   __k_SIGXCPU
#define SIGXFSZ   __k_SIGXFSZ

#define SIGRTMIN __k_SIGRTMIN
#define SIGRTMAX __k_SIGRTMAX

#define NSIG __k_NSIG

#define SIG_DFL __k_SIG_DFL
#define SIG_ERR __k_SIG_ERR
#define SIG_IGN __k_SIG_IGN

#define SA_NOCLDSTOP __k_SA_NOCLDSTOP

#define SA_NOCLDWAIT __k_SA_NOCLDWAIT
#define SA_SIGINFO   __k_SA_SIGINFO
#define SA_ONSTACK   __k_SA_ONSTACK
#define SA_RESTART   __k_SA_RESTART
#define SA_NODEFER   __k_SA_NODEFER
#define SA_RESETHAND __k_SA_RESETHAND

typedef __k_sighandler_t sighandler_t;
typedef __k_sigset_t sigset_t;

#define k_sigaction __k_sigaction

struct process;
struct regfile;
struct thread;

bool signal_pending(struct thread* restrict thrd);
void signal_deliver(struct thread* restrict thrd, struct regfile* restrict regs);

void signal_psend(struct process* restrict proc, int signum);
void signal_tsend(struct thread* restrict thrd, int signum);

void signal_return(struct thread* restrict thrd, struct regfile* restrict regs);

#endif /* E9456F4D_FA9E_4E06_9DA4_B0D7765C0CAC */
