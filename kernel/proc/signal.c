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
    int sig = info->si_signo;

    if(sig == SIGKILL) {
        process_kill(proc, SIGKILL);
        return 0;
    }

    // Allocated up front, since there is no sleeping under the lock;
    // without it the signal still arrives, just with no siginfo
    struct sigqueue* node = slab_alloc(sizeof(struct sigqueue));

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
        return 0;
    }

    *pending |= bit;

    if(node) {
        node->info = *info;
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

    return 0;
}

void signal_force(struct thread* restrict thrd, const struct k_siginfo* restrict info)
{
    struct process* proc = thrd->proc;
    struct sighand* sighand = proc->sighand;
    int sig = info->si_signo;

    bool intr_state = spinlock_acquire_intr(&sighand->lock);

    struct k_sigaction* action = &sighand->actions[sig - 1];

    if(action->sa_handler == SIG_IGN) {
        action->sa_handler = SIG_DFL;
    }

    thrd->sig_blocked &= ~sigbit(sig);

    spinlock_release_intr(&sighand->lock, intr_state);

    signal_send(proc, thrd, info);

    // The signal may have been pending
    // already, so the send set no work
    intr_state = spinlock_acquire_intr(&sighand->lock);
    recalc_locked(thrd);
    spinlock_release_intr(&sighand->lock, intr_state);
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
    sighandler_t handler = sighand->actions[sig - 1].sa_handler;

    recalc_locked(thrd);

    spinlock_release_intr(&sighand->lock, intr_state);

    slab_free(node);

    // Handlers in userspace come with signal frames
    if(handler == SIG_DFL && default_kills(sig)) {
        process_kill(proc, sig);
    }
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

void signal_flush(struct sigqueue* restrict queue)
{
    while(queue) {
        struct sigqueue* next = queue->next;
        slab_free(queue);
        queue = next;
    }
}
