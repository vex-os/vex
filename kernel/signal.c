#include <vex.arch/mctx.h>
#include <vex/kprintf.h>
#include <vex/process.h>
#include <vex/signal.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vmm.h>

#define SIG_UNCATCHABLE ((UINT64_C(1) << SIGKILL) | (UINT64_C(1) << SIGSTOP))

static __always_inline __nodiscard sigset_t signal_bit(int signum)
{
    return ((sigset_t)(1)) << (unsigned)(signum);
}

static void __noreturn nuke_process(struct process* restrict proc, struct thread* restrict culprit, int signum)
{
    pr_warning("signal: pid=%lu sig=%d: killed, culprit thread=%lu", (unsigned long)(proc->pid), signum, (unsigned long)(culprit->index));

    for(struct thread* it = proc->threads; it; it = it->proc_next) {
        if(it != culprit) {
            it->state = THREAD_DEAD;
        }
    }

    culprit->state = THREAD_SEGV;
    thread_exit_handler(-signum);
    unreachable();
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
    struct pagemap* vm = thrd->owner->vm;
    uintptr_t user_sp = regs->regfile_sp;

    user_sp -= sizeof(struct k_ucontext);
    user_sp = align_floor(user_sp, 16);
    uintptr_t uctx_uaddr = user_sp;

    user_sp -= sizeof(struct k_siginfo);
    uintptr_t siginfo_uaddr = user_sp;

    user_sp -= sizeof(uintptr_t);
    uintptr_t uctx_ptr_uaddr = user_sp;

    user_sp -= sizeof(uintptr_t);
    uintptr_t ret_uaddr = user_sp;

    struct k_ucontext uctx;
    memset(&uctx, 0, sizeof(uctx));
    regfile_to_mcontext(regs, &uctx.uc_mcontext);
    uctx.uc_sigmask = thrd->sig_blocked;

    // If a signal handler does a conventional return (which is an objectively retarded take
    // on whoever may decide to do this) instead of a proper sys_sigreturn, the process will
    // jump to SIGRETURN_GUARD_VIRT and call sys_exit with a non-zero exit code instead of
    // faulting into an infinite SIGSEGV loop; the guard page is mapped as VPROT_URX
    uintptr_t guard_virt = SIGRETURN_GUARD_VIRT;

    size_t check = 0;
    check += memcpy_sys_to_vm(vm, uctx_uaddr, &uctx, sizeof(uctx));
    check += memcpy_sys_to_vm(vm, siginfo_uaddr, siginfo_src, sizeof(struct k_siginfo));
    check += memcpy_sys_to_vm(vm, uctx_ptr_uaddr, &uctx_uaddr, sizeof(uintptr_t));
    check += memcpy_sys_to_vm(vm, ret_uaddr, &guard_virt, sizeof(uintptr_t));

    if(check != 0) {
        pr_warning("signal: pid=%lu sig=%d: bad stack pointer %p, killing process", (unsigned long)(thrd->owner->pid),
            (int)(siginfo_src->si_signo), (void*)(regs->regfile_sp));
        nuke_process(thrd->owner, thrd, SIGSEGV);
        unreachable();
    }

    regs->regfile_sp = user_sp;
    regs->regfile_arg_0 = (uintptr_t)(siginfo_src->si_signo);
    regs->regfile_arg_1 = siginfo_uaddr;
    regs->regfile_arg_2 = uctx_uaddr;
}

bool signal_pending(struct thread* restrict thrd)
{
    if(thrd->owner == nullptr) {
        return false;
    }

    return lowest_signum(thrd) != 0;
}

void signal_deliver_base(struct thread* restrict thrd, struct regfile* restrict regs, const struct k_siginfo* restrict siginfo_src)
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
        pr_verbose("signal: pid=%lu sig=%d: ignored", (unsigned long)(thrd->owner->pid), (int)(siginfo_src->si_signo));
        return;
    }

    if(action->sa_handler == SIG_DFL) {
        nuke_process(thrd->owner, thrd, siginfo_src->si_signo);
        unreachable();
    }

    if(0 == (action->sa_flags & SA_NODEFER)) {
        thrd->sig_blocked |= bit;
    }

    thrd->sig_blocked |= action->sa_mask;
    thrd->sig_blocked &= ~SIG_UNCATCHABLE;

    push_frame(thrd, regs, siginfo_src);

    if(action->sa_flags & SA_SIGINFO) {
        pr_verbose("signal: pid=%lu sig=%d: sigaction=%p", (unsigned long)(thrd->owner->pid), (int)(siginfo_src->si_signo),
            (void*)(action->sa_sigaction));
        regs->regfile_pc = (uintptr_t)(action->sa_sigaction);
    }
    else {
        pr_verbose("signal: pid=%lu sig=%d: handler=%p", (unsigned long)(thrd->owner->pid), (int)(siginfo_src->si_signo),
            (void*)(action->sa_handler));
        regs->regfile_pc = (uintptr_t)(action->sa_handler);
    }

    if(action->sa_flags & SA_RESETHAND) {
        action->sa_handler = SIG_DFL;
        action->sa_flags = 0;
    }
}

void signal_deliver_sync(struct thread* restrict thrd, struct regfile* restrict regs, const struct k_siginfo* restrict siginfo_src)
{
    sigset_t bit = signal_bit(siginfo_src->si_signo);

    if(thrd->sig_blocked & bit) {
        pr_warning("signal: pid=%lu sig=%d: signal raised twice, killing process", (unsigned long)(thrd->owner->pid),
            (int)(siginfo_src->si_signo));
        nuke_process(thrd->owner, thrd, siginfo_src->si_signo);
        unreachable();
    }

    signal_deliver_base(thrd, regs, siginfo_src);
}

void signal_deliver_auto(struct thread* restrict thrd, struct regfile* restrict regs)
{
    struct k_siginfo siginfo_src;
    memset(&siginfo_src, 0, sizeof(struct k_siginfo));

    siginfo_src.si_signo = lowest_signum(thrd);
    siginfo_src.si_code = SI_USER;

    if(thrd->owner) {
        siginfo_src.si_pid = thrd->owner->pid;
    }

    signal_deliver_base(thrd, regs, &siginfo_src);
}

void signal_return(struct thread* restrict thrd, struct regfile* restrict regs)
{
    uintptr_t uctx_uaddr = regs->syscall_arg_0;
    struct k_ucontext uctx;

    if(memcpy_vm_to_sys(&uctx, thrd->owner->vm, uctx_uaddr, sizeof(uctx))) {
        pr_warning("signal: pid=%lu: bad ucontext pointer %p, killing process", (unsigned long)(thrd->owner->pid), (void*)(uctx_uaddr));
        nuke_process(thrd->owner, thrd, SIGSEGV);
        unreachable();
    }

    mcontext_to_regfile(&uctx.uc_mcontext, regs);
    thrd->sig_blocked = uctx.uc_sigmask & ~SIG_UNCATCHABLE;
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
