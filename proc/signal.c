#include <vex.arch/regs.h>
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

static struct regfile* push_frame(struct regfile* restrict regs, int signum)
{
    uintptr_t stack_top = regs->regfile_sp;
    stack_top -= sizeof(struct regfile);
    stack_top = align_floor(stack_top, 16);

    struct regfile* saved = (struct regfile*)(stack_top);
    memcpy(saved, regs, sizeof(struct regfile));

    stack_top -= sizeof(uintptr_t);

    uintptr_t* return_addr = (uintptr_t*)(stack_top);
    return_addr[0] = 0;

    regs->regfile_sp = stack_top;
    regs->regfile_arg_0 = signum;

    return saved;
}

bool signal_pending(struct thread* restrict thrd)
{
    if(thrd->owner == nullptr) {
        return false;
    }

    return lowest_signum(thrd);
}

void signal_deliver(struct thread* restrict thrd, struct regfile* restrict regs)
{
    int signum = lowest_signum(thrd);

    if(signum == 0) {
        return;
    }

    sigset_t bit = signal_bit(signum);

    if(thrd->sig_pending & bit) {
        thrd->sig_pending &= ~bit;
    }
    else {
        thrd->owner->sig_pending &= ~bit;
    }

    struct k_sigaction* action = &thrd->owner->sig_actions[signum];

    if(action->sa_handler == SIG_IGN) {
        pr_verbose("signal: pid=%u sig=%d ignored", thrd->owner->pid, signum);
        return;
    }

    if(action->sa_handler == SIG_DFL) {
        pr_warning("signal: pid=%u sig=%d default action: terminate", thrd->owner->pid, signum);

        for(struct thread* it = thrd->owner->threads; it; it = it->proc_next) {
            if(it != thrd) {
                it->state = THREAD_DEAD;
            }
        }

        thrd->state = THREAD_SEGV;
        thread_exit_handler(-signum);
        unreachable();
    }

    pr_verbose("signal: pid=%u sig=%d handler=%p", thrd->owner->pid, signum, (void*)(action->sa_handler));

    thrd->sig_return_regs = push_frame(regs, signum);
    thrd->sig_return_blocked = thrd->sig_blocked;

    if(0 == (action->sa_flags & SA_NODEFER)) {
        thrd->sig_blocked |= bit;
    }

    thrd->sig_blocked |= action->sa_mask;
    thrd->sig_blocked &= ~SIG_UNCATCHABLE;

    regs->regfile_pc = (uintptr_t)(action->sa_handler);

    if(action->sa_flags & SA_RESETHAND) {
        action->sa_handler = SIG_DFL;
        action->sa_flags = 0;
    }
}

void signal_return(struct thread* restrict thrd, struct regfile* restrict regs)
{
    if(thrd->sig_return_regs) {
        memcpy(regs, thrd->sig_return_regs, sizeof(struct regfile));
        thrd->sig_blocked = thrd->sig_return_blocked & ~SIG_UNCATCHABLE;
        thrd->sig_return_regs = nullptr;
        thrd->sig_return_blocked = 0;
    }
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
