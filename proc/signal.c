#include <vex.arch/mctx.h>
#include <vex/kprintf.h>
#include <vex/process.h>
#include <vex/signal.h>
#include <vex/string.h>
#include <vex/thread.h>

#define SIG_UNCATCHABLE ((UINT64_C(1) << SIGKILL) | (UINT64_C(1) << SIGSTOP))

static __always_inline __nodiscard sigset_t signal_bit(int signum)
{
    return ((sigset_t)(1)) << (unsigned)(signum);
}

static int lowest_signum(struct thread* restrict thrd)
{
    sigset_t pending = (thrd->sig_pending | thrd->owner->sig_pending) & ~thrd->sig_blocked;

    if(pending == 0) {
        return 0;
    }

    return __builtin_ctzll(pending);
}

static void push_frame(struct thread* restrict thrd, struct regfile* restrict regs, const struct k_siginfo* restrict siginfo_src)
{
    uintptr_t user_stack = regs->regfile_sp;

    user_stack -= sizeof(struct k_ucontext);
    user_stack = align_floor(user_stack, 16);
    struct k_ucontext* uctx = (struct k_ucontext*)(user_stack);

    user_stack -= sizeof(struct k_siginfo);
    struct k_siginfo* siginfo = (struct k_siginfo*)(user_stack);

    user_stack -= sizeof(uintptr_t);
    uintptr_t* uctx_ptr = (uintptr_t*)(user_stack);
    uctx_ptr[0] = (uintptr_t)(uctx);

    user_stack -= sizeof(uintptr_t);
    uintptr_t* ret_addr = (uintptr_t*)(user_stack);
    ret_addr[0] = 0;

    memset(uctx, 0, sizeof(struct k_ucontext));
    regfile_to_mcontext(regs, &uctx->uc_mcontext);
    uctx->uc_sigmask = thrd->sig_blocked;

    memcpy(siginfo, siginfo_src, sizeof(struct k_siginfo));

    regs->regfile_sp = user_stack;
    regs->regfile_arg_0 = (uintptr_t)(siginfo_src->si_signo);
    regs->regfile_arg_1 = (uintptr_t)(siginfo);
    regs->regfile_arg_2 = (uintptr_t)(uctx);
}

bool signal_pending(struct thread* restrict thrd)
{
    if(thrd->owner == nullptr) {
        return false;
    }

    return lowest_signum(thrd) != 0;
}

void signal_deliver(struct thread* restrict thrd, struct regfile* restrict regs)
{
    struct k_siginfo siginfo_src;
    memset(&siginfo_src, 0, sizeof(struct k_siginfo));

    siginfo_src.si_signo = lowest_signum(thrd);
    siginfo_src.si_code = SI_USER;

    if(thrd->owner) {
        siginfo_src.si_pid = thrd->owner->pid;
    }

    signal_deliver_ex(thrd, regs, &siginfo_src);
}

void signal_deliver_ex(struct thread* restrict thrd, struct regfile* restrict regs, const struct k_siginfo* restrict siginfo_src)
{
    sigset_t bit = signal_bit(siginfo_src->si_signo);

    if(thrd->sig_pending & bit) {
        thrd->sig_pending &= ~bit;
    }
    else {
        thrd->owner->sig_pending &= ~bit;
    }

    struct k_sigaction* action = &thrd->owner->sig_actions[siginfo_src->si_signo];

    if(action->sa_handler == SIG_IGN) {
        pr_verbose("signal: pid=%u sig=%d ignored", thrd->owner->pid, siginfo_src->si_signo);
        return;
    }

    if(action->sa_handler == SIG_DFL) {
        pr_warning("signal: pid=%u sig=%d default action: terminate", thrd->owner->pid, siginfo_src->si_signo);

        for(struct thread* it = thrd->owner->threads; it; it = it->proc_next) {
            if(it != thrd) {
                it->state = THREAD_DEAD;
            }
        }

        thrd->state = THREAD_SEGV;
        thread_exit_handler(-siginfo_src->si_signo);
        unreachable();
    }

    if(0 == (action->sa_flags & SA_NODEFER)) {
        thrd->sig_blocked |= bit;
    }

    thrd->sig_blocked |= action->sa_mask;
    thrd->sig_blocked &= ~SIG_UNCATCHABLE;

    push_frame(thrd, regs, siginfo_src);

    if(action->sa_flags & SA_SIGINFO) {
        pr_verbose("signal: pid=%u sig=%d sigaction=%p", thrd->owner->pid, siginfo_src->si_signo, (void*)(action->sa_sigaction));
        regs->regfile_pc = (uintptr_t)(action->sa_sigaction);
    }
    else {
        pr_verbose("signal: pid=%u sig=%d handler=%p", thrd->owner->pid, siginfo_src->si_signo, (void*)(action->sa_handler));
        regs->regfile_pc = (uintptr_t)(action->sa_handler);
    }

    if(action->sa_flags & SA_RESETHAND) {
        action->sa_handler = SIG_DFL;
        action->sa_flags = 0;
    }
}

void signal_return(struct thread* restrict thrd, struct regfile* restrict regs)
{
    struct __k_ucontext* uctx = (struct __k_ucontext*)(regs->syscall_arg_0);

    // TODO: validate uctx is within the process user address space

    mcontext_to_regfile(&uctx->uc_mcontext, regs);
    thrd->sig_blocked = uctx->uc_sigmask & ~SIG_UNCATCHABLE;
}

void signal_psend(struct process* restrict proc, int signum)
{
    if(signum <= 0 || signum >= NSIG) {
        return;
    }

    proc->sig_pending |= signal_bit(signum);
}

void signal_tsend(struct thread* restrict thrd, int signum)
{
    if(signum <= 0 || signum >= NSIG) {
        return;
    }

    thrd->sig_pending |= signal_bit(signum);
}
