#include <vex/atomic.h>
#include <vex/page.h>
#include <vex/process.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/signal.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/thread.h>

static_assert(sizeof(struct sighand) <= PAGE_SIZE);

enum {
    DEFAULT_TERM,
    DEFAULT_CORE,
    DEFAULT_IGNORE,
    DEFAULT_STOP,
    DEFAULT_CONT,
};

static const uint8_t default_actions[NSIG] = {
    [SIGQUIT - 1] = DEFAULT_CORE,
    [SIGILL - 1] = DEFAULT_CORE,
    [SIGTRAP - 1] = DEFAULT_CORE,
    [SIGABRT - 1] = DEFAULT_CORE,
    [SIGBUS - 1] = DEFAULT_CORE,
    [SIGFPE - 1] = DEFAULT_CORE,
    [SIGSEGV - 1] = DEFAULT_CORE,
    [SIGCHLD - 1] = DEFAULT_IGNORE,
    [SIGCONT - 1] = DEFAULT_CONT,
    [SIGSTOP - 1] = DEFAULT_STOP,
    [SIGTSTP - 1] = DEFAULT_STOP,
    [SIGTTIN - 1] = DEFAULT_STOP,
    [SIGTTOU - 1] = DEFAULT_STOP,
    [SIGURG - 1] = DEFAULT_IGNORE,
    [SIGXCPU - 1] = DEFAULT_CORE,
    [SIGXFSZ - 1] = DEFAULT_CORE,
    [SIGWINCH - 1] = DEFAULT_IGNORE,
    [SIGSYS - 1] = DEFAULT_CORE,
};

static bool default_kills(int sig)
{
    uint8_t action = default_actions[sig - 1];
    return action == DEFAULT_TERM || action == DEFAULT_CORE;
}

static bool ignored_locked(const struct sighand* restrict sighand, int sig)
{
    sighandler_t handler = sighand->actions[sig - 1].sa_handler;
    return handler == SIG_IGN || (handler == SIG_DFL && !default_kills(sig));
}

static void queue_append(struct sigqueue** restrict queue, struct sigqueue* restrict node)
{
    while(*queue) {
        queue = &(*queue)->next;
    }

    node->next = nullptr;
    *queue = node;
}

static struct sigqueue* queue_take(struct sigqueue** restrict queue, int sig)
{
    for(; *queue; queue = &(*queue)->next) {
        struct sigqueue* node = *queue;

        if(node->info.si_signo == sig) {
            *queue = node->next;
            return node;
        }
    }

    return nullptr;
}

static void recalc_locked(struct thread* restrict thrd)
{
    struct process* proc = thrd->proc;
    sigset_t deliverable = (thrd->sig_pending | proc->sig_pending) & ~thrd->sig_blocked;

    if(deliverable) {
        atomic_fetch_or(&thrd->pending_work, THREAD_WORK_SIGNAL, ATOMIC_RELEASE);
    }
    else {
        atomic_fetch_and(&thrd->pending_work, ~THREAD_WORK_SIGNAL, ATOMIC_RELEASE);
    }
}

static struct thread* choose_thread_locked(struct process* restrict proc, sigset_t bit)
{
    struct thread* self = this_thread();

    if(self->proc == proc && 0 == (self->sig_blocked & bit)) {
        return self;
    }

    for(struct thread* thrd = proc->threads; thrd; thrd = thrd->tnext) {
        if(0 == (thrd->sig_blocked & bit)) {
            return thrd;
        }
    }

    return nullptr;
}

static void drop_locked(sigset_t* restrict pending, struct sigqueue** restrict queue, int sig, struct sigqueue** restrict dropped)
{
    *pending &= ~sigbit(sig);

    for(struct sigqueue* node = queue_take(queue, sig); node; node = queue_take(queue, sig)) {
        node->next = *dropped;
        *dropped = node;
    }
}

static void queue_signal(struct process* restrict proc, struct thread* restrict target, int sig, struct sigqueue* restrict node)
{
    struct sighand* sighand = proc->sighand;
    sigset_t bit = sigbit(sig);

    sigset_t* pending = &proc->sig_pending;
    struct sigqueue** queue = &proc->sig_queue;

    if(target) {
        pending = &target->sig_pending;
        queue = &target->sig_queue;
    }

    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    bool blocked = target && 0 != (target->sig_blocked & bit);

    // A blocked signal stays pending even if ignored: the action may change before it is unblocked
    if((ignored_locked(sighand, sig) && !blocked) || (*pending & bit)) {
        spinlock_release_intr(&sighand->lock, intr_state);
        slab_free(node);
        return;
    }

    *pending |= bit;

    if(node) {
        queue_append(queue, node);
    }

    bool proc_intr_state = spinlock_acquire_intr(&proc->lock);

    struct thread* thrd = target;

    if(thrd == nullptr) {
        thrd = choose_thread_locked(proc, bit);
    }

    if(thrd && 0 == (thrd->sig_blocked & bit)) {
        atomic_fetch_or(&thrd->pending_work, THREAD_WORK_SIGNAL, ATOMIC_RELEASE);
        sched_kick(thrd);
    }

    spinlock_release_intr(&proc->lock, proc_intr_state);
    spinlock_release_intr(&sighand->lock, intr_state);
}

struct sighand* sighand_create(struct sighand* restrict parent)
{
    struct sighand* sighand = pslab_alloc_virt();

    if(sighand == nullptr) {
        return nullptr;
    }

    // SIG_DFL with no flags and an empty mask is all zeroes
    memset(sighand, 0, sizeof(struct sighand));

    if(parent) {
        bool intr_state = spinlock_acquire_intr(&parent->lock);
        memcpy(sighand->actions, parent->actions, sizeof(sighand->actions));
        spinlock_release_intr(&parent->lock, intr_state);
    }

    sighand->lock = SPINLOCK_INIT;

    return sighand;
}

void sighand_destroy(struct sighand* restrict sighand)
{
    pslab_free_virt(sighand);
}

void sighand_exec(struct sighand* restrict sighand)
{
    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    for(int i = 0; i < NSIG; ++i) {
        struct k_sigaction* action = &sighand->actions[i];

        if(action->sa_handler == SIG_IGN) {
            action->sa_flags = 0;
            action->sa_mask = 0;
            continue;
        }

        memset(action, 0, sizeof(struct k_sigaction));
    }

    spinlock_release_intr(&sighand->lock, intr_state);
}

int signal_send(struct process* restrict proc, struct thread* restrict target, const struct k_siginfo* restrict info)
{
    if(info->si_signo == SIGKILL) {
        process_kill(proc, SIGKILL);
        return 0;
    }

    // Allocated before the lock, under which nothing sleeps;
    // without it the signal still arrives, just with no siginfo
    struct sigqueue* node = slab_alloc(sizeof(struct sigqueue));

    if(node) {
        node->info = *info;
    }

    queue_signal(proc, target, info->si_signo, node);

    return 0;
}

// For senders that cannot allocate, such as the scheduler; the node is consumed either way
void signal_send_node(struct process* restrict proc, struct sigqueue* restrict node)
{
    queue_signal(proc, nullptr, node->info.si_signo, node);
}

void signal_force(struct thread* restrict thrd, const struct k_siginfo* restrict info)
{
    struct process* proc = thrd->proc;
    struct sighand* sighand = proc->sighand;
    int sig = info->si_signo;
    sigset_t bit = sigbit(sig);
    struct sigqueue* dropped = nullptr;

    struct sigqueue* node = slab_alloc(sizeof(struct sigqueue));

    if(node) {
        node->info = *info;
    }

    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    struct k_sigaction* action = &sighand->actions[sig - 1];

    // A blocked signal means its handler is the one that faulted, or the fault
    // came with the handler masked; running it would only fault again
    if(action->sa_handler == SIG_IGN || 0 != (thrd->sig_blocked & bit)) {
        memset(action, 0, sizeof(struct k_sigaction));
    }

    thrd->sig_blocked &= ~bit;

    // The fault's own siginfo replaces whatever was pending for it already
    drop_locked(&thrd->sig_pending, &thrd->sig_queue, sig, &dropped);
    thrd->sig_pending |= bit;

    if(node) {
        queue_append(&thrd->sig_queue, node);
    }

    recalc_locked(thrd);

    spinlock_release_intr(&sighand->lock, intr_state);

    signal_flush(dropped);
}

void signal_deliver(struct hart_context* restrict ctx)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;
    struct sighand* sighand = proc->sighand;

    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    sigset_t* pending = &thrd->sig_pending;
    struct sigqueue** queue = &thrd->sig_queue;
    sigset_t deliverable = *pending & ~thrd->sig_blocked;

    if(deliverable == 0) {
        pending = &proc->sig_pending;
        queue = &proc->sig_queue;
        deliverable = *pending & ~thrd->sig_blocked;
    }

    if(deliverable == 0) {
        recalc_locked(thrd);
        spinlock_release_intr(&sighand->lock, intr_state);
        return;
    }

    int sig = __builtin_ctzll(deliverable) + 1;

    *pending &= ~sigbit(sig);

    struct sigqueue* node = queue_take(queue, sig);
    struct k_sigaction action = sighand->actions[sig - 1];
    sigset_t old_mask = thrd->sig_blocked;
    bool caught = action.sa_handler != SIG_DFL && action.sa_handler != SIG_IGN;

    if(caught && (action.sa_flags & SA_RESETHAND)) {
        memset(&sighand->actions[sig - 1], 0, sizeof(struct k_sigaction));
    }

    recalc_locked(thrd);

    spinlock_release_intr(&sighand->lock, intr_state);

    struct k_siginfo info = { 0 };
    info.si_signo = sig;
    info.si_code = SI_KERNEL;

    if(node) {
        info = node->info;
        slab_free(node);
    }

    if(!caught) {
        if(action.sa_handler == SIG_DFL && default_kills(sig)) {
            process_kill(proc, sig);
        }

        return;
    }

    if(thrd->syscall_restart && 0 == (action.sa_flags & SA_RESTART)) {
        arch_syscall_interrupted(ctx);
    }

    thrd->syscall_restart = false;

    // A stack that takes no frame leaves nothing to return to
    if(arch_signal_setup(thrd, ctx, &action, &info, old_mask) < 0) {
        process_kill(proc, SIGSEGV);
        return;
    }

    sigset_t blocked = action.sa_mask;

    if(0 == (action.sa_flags & SA_NODEFER)) {
        blocked |= sigbit(sig);
    }

    signal_mask(thrd, SIG_BLOCK, blocked);
}

void signal_action(struct process* restrict proc, int sig, const struct k_sigaction* restrict act, struct k_sigaction* restrict oact)
{
    struct sighand* sighand = proc->sighand;
    struct sigqueue* dropped = nullptr;

    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    if(oact) {
        *oact = sighand->actions[sig - 1];
    }

    if(act) {
        sighand->actions[sig - 1] = *act;

        if(ignored_locked(sighand, sig)) {
            drop_locked(&proc->sig_pending, &proc->sig_queue, sig, &dropped);

            bool proc_intr_state = spinlock_acquire_intr(&proc->lock);

            for(struct thread* thrd = proc->threads; thrd; thrd = thrd->tnext) {
                drop_locked(&thrd->sig_pending, &thrd->sig_queue, sig, &dropped);
            }

            spinlock_release_intr(&proc->lock, proc_intr_state);
        }
    }

    spinlock_release_intr(&sighand->lock, intr_state);

    signal_flush(dropped);
}

sigset_t signal_mask(struct thread* restrict thrd, int how, sigset_t set)
{
    struct process* proc = thrd->proc;
    struct sighand* sighand = proc->sighand;

    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    sigset_t old = thrd->sig_blocked;
    sigset_t blocked = set;

    if(how == SIG_BLOCK) {
        blocked = old | set;
    }
    else if(how == SIG_UNBLOCK) {
        blocked = old & ~set;
    }

    thrd->sig_blocked = blocked & ~(sigbit(SIGKILL) | sigbit(SIGSTOP));

    // Unblocking may leave something
    // deliverable on the way out
    recalc_locked(thrd);

    spinlock_release_intr(&sighand->lock, intr_state);

    return old;
}

sigset_t signal_pending(struct thread* restrict thrd)
{
    struct process* proc = thrd->proc;
    struct sighand* sighand = proc->sighand;

    bool intr_state = spinlock_acquire_intr(&sighand->lock);
    sigset_t pending = (thrd->sig_pending | proc->sig_pending) & thrd->sig_blocked;
    spinlock_release_intr(&sighand->lock, intr_state);

    return pending;
}

bool signal_on_altstack(const struct thread* restrict thrd, uintptr_t sp)
{
    uintptr_t base = thrd->sig_altstack;

    return thrd->sig_altstack_size && sp > base && sp - base <= thrd->sig_altstack_size;
}

void signal_altstack_get(const struct thread* restrict thrd, uintptr_t sp, struct k_stack* restrict out)
{
    out->ss_sp = (void*)(thrd->sig_altstack);
    out->ss_size = thrd->sig_altstack_size;
    out->ss_flags = 0;
    out->ss_reserved = 0;

    if(thrd->sig_altstack_size == 0) {
        out->ss_flags = SS_DISABLE;
    }
    else if(signal_on_altstack(thrd, sp)) {
        out->ss_flags = SS_ONSTACK;
    }
}

void signal_flush(struct sigqueue* restrict queue)
{
    while(queue) {
        struct sigqueue* next = queue->next;
        slab_free(queue);
        queue = next;
    }
}
