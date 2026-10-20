#include <vex/capability.h>
#include <vex/cred.h>
#include <vex/errno.h>
#include <vex/process.h>
#include <vex/signal.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>

#define SA_KNOWN (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_SIGINFO | SA_ONSTACK | SA_RESTART | SA_NODEFER | SA_RESETHAND | SA_RESTORER)

static bool may_signal(const struct credentials* restrict sender, const struct credentials* restrict target)
{
    if(cred_capable(sender, CAP_KILL)) {
        return true;
    }

    return sender->uid == target->uid || sender->uid == target->suid || sender->euid == target->uid || sender->euid == target->suid;
}

static intptr_t sys_kill(const uintptr_t* restrict args)
{
    pid_t pid = args[0];
    int sig = args[1];

    struct thread* thrd = this_thread();
    struct process* sender = thrd->proc;

    if(sender == nullptr) {
        return -ESRCH;
    }

    if(sig < 0 || sig > NSIG) {
        return -EINVAL;
    }

    if(pid <= 0) {
        return -EINVAL; // Process groups don't exist yet
    }

    struct process* target = process_locate(pid);

    if(target == nullptr) {
        return -ESRCH;
    }

    struct credentials* target_cred = process_get_cred(target);
    bool allowed = may_signal(thrd->cred, target_cred);
    cred_decref(target_cred);

    intptr_t retval = -EPERM;

    if(allowed) {
        retval = 0;

        if(sig) {
            struct k_siginfo info = { 0 };
            info.si_signo = sig;
            info.si_code = SI_USER;
            info.si_pid = sender->pid;
            info.si_uid = thrd->cred->uid;

            retval = signal_send(target, nullptr, &info);
        }
    }

    process_decref(target);

    return retval;
}

static intptr_t sys_sigaction(const uintptr_t* restrict args)
{
    int sig = args[0];
    uintptr_t user_act = args[1];
    uintptr_t user_oact = args[2];

    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    if(sig < 1 || sig > NSIG) {
        return -EINVAL;
    }

    struct k_sigaction act;
    struct k_sigaction oact;

    if(user_act) {
        ssize_t copied = user_copyin(&act, user_act, sizeof(act));

        if(copied < 0) {
            return copied;
        }

        if(sig == SIGKILL || sig == SIGSTOP || 0 != (act.sa_flags & ~SA_KNOWN)) {
            return -EINVAL;
        }

        // There is no trampoline of the kernel's own to return through
        if(act.sa_handler != SIG_DFL && act.sa_handler != SIG_IGN && 0 == (act.sa_flags & SA_RESTORER)) {
            return -EINVAL;
        }

        act.sa_mask &= ~(sigbit(SIGKILL) | sigbit(SIGSTOP));
    }

    signal_action(proc, sig, user_act ? &act : nullptr, &oact);

    if(user_oact) {
        ssize_t copied = user_copyout(user_oact, &oact, sizeof(oact));

        if(copied < 0) {
            return copied;
        }
    }

    return 0;
}

static intptr_t sys_sigprocmask(const uintptr_t* restrict args)
{
    int how = args[0];
    uintptr_t user_set = args[1];
    uintptr_t user_oset = args[2];

    struct thread* thrd = this_thread();

    if(thrd->proc == nullptr) {
        return -ENOSYS;
    }

    sigset_t set = 0;

    if(user_set) {
        if(how != SIG_BLOCK && how != SIG_UNBLOCK && how != SIG_SETMASK) {
            return -EINVAL;
        }

        ssize_t copied = user_copyin(&set, user_set, sizeof(set));

        if(copied < 0) {
            return copied;
        }
    }
    else {
        how = SIG_BLOCK; // Blocking nothing more only reads the mask
    }

    sigset_t oset = signal_mask(thrd, how, set);

    if(user_oset) {
        ssize_t copied = user_copyout(user_oset, &oset, sizeof(oset));

        if(copied < 0) {
            return copied;
        }
    }

    return 0;
}

static intptr_t sys_sigpending(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();

    if(thrd->proc == nullptr) {
        return -ENOSYS;
    }

    sigset_t pending = signal_pending(thrd);
    ssize_t copied = user_copyout(args[0], &pending, sizeof(pending));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

static intptr_t sys_sigreturn(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    struct hart_context* ctx = thrd->syscall_ctx;
    sigset_t mask;

    if(arch_signal_return(thrd, ctx, &mask) < 0) {
        process_kill(proc, SIGSEGV);
        return 0;
    }

    signal_mask(thrd, SIG_SETMASK, mask);

    // The return value lands in the register just restored
    return ctx->syscall_retval;
}

static intptr_t sys_sigaltstack(const uintptr_t* restrict args)
{
    uintptr_t user_ss = args[0];
    uintptr_t user_old = args[1];

    struct thread* thrd = this_thread();

    if(thrd->proc == nullptr) {
        return -ENOSYS;
    }

    struct hart_context* ctx = thrd->syscall_ctx;
    bool on_altstack = signal_on_altstack(thrd, ctx->syscall_sp);

    struct k_stack old = { 0 };
    old.ss_sp = (void*)(thrd->sig_altstack);
    old.ss_size = thrd->sig_altstack_size;

    if(thrd->sig_altstack_size == 0) {
        old.ss_flags = SS_DISABLE;
    }
    else if(on_altstack) {
        old.ss_flags = SS_ONSTACK;
    }

    if(user_ss) {
        struct k_stack ss;
        ssize_t copied = user_copyin(&ss, user_ss, sizeof(ss));

        if(copied < 0) {
            return copied;
        }

        if(on_altstack) {
            return -EPERM;
        }

        if(ss.ss_flags & ~SS_DISABLE) {
            return -EINVAL;
        }

        if(ss.ss_flags & SS_DISABLE) {
            thrd->sig_altstack = 0;
            thrd->sig_altstack_size = 0;
        }
        else {
            uintptr_t base = (uintptr_t)(ss.ss_sp);

            if(ss.ss_size < MINSIGSTKSZ || ss.ss_size > UINT32_MAX) {
                return -ENOMEM;
            }

            if(base + ss.ss_size < base) {
                return -EINVAL;
            }

            thrd->sig_altstack = base;
            thrd->sig_altstack_size = ss.ss_size;
        }
    }

    if(user_old) {
        ssize_t copied = user_copyout(user_old, &old, sizeof(old));

        if(copied < 0) {
            return copied;
        }
    }

    return 0;
}

define_syscall(sys_kill, SYS_KILL);
define_syscall(sys_sigaction, SYS_SIGACTION);
define_syscall(sys_sigprocmask, SYS_SIGPROCMASK);
define_syscall(sys_sigpending, SYS_SIGPENDING);
define_syscall(sys_sigreturn, SYS_SIGRETURN);
define_syscall(sys_sigaltstack, SYS_SIGALTSTACK);
