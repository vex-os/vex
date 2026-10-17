#include <arch/ireq.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/clockevt.h>
#include <vex/clocksrc.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kmalloc.h>
#include <vex/kstack.h>
#include <vex/limits.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/preempt.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vms.h>

static_assert(sizeof(struct thread) <= SLAB_MAX_SIZE);

#define SCHED_TIMESLICE (UINT64_C(CONFIG_SCHED_TIMESLICE) * 1000) // In nanoseconds

// Holds RUNNABLE threads only; the lock also covers every thread's state
static spinlock_type sched_lock = SPINLOCK_INIT;
static struct thread* rq_head;
static struct thread* rq_tail;

static unsigned sched_ireq_number;

// Threads waiting with a deadline, soonest first; taken
// inside event locks and outside sched_lock
static spinlock_type sleep_lock = SPINLOCK_INIT;
static struct thread* sleep_head;

static void rq_push_nolock(struct thread* restrict thrd)
{
    thrd->rq_next = nullptr;
    thrd->rq_prev = rq_tail;

    if(rq_tail) {
        rq_tail->rq_next = thrd;
    }
    else {
        rq_head = thrd;
    }

    rq_tail = thrd;
}

static void rq_remove_nolock(struct thread* restrict thrd)
{
    if(thrd->rq_prev) {
        thrd->rq_prev->rq_next = thrd->rq_next;
    }
    else {
        rq_head = thrd->rq_next;
    }

    if(thrd->rq_next) {
        thrd->rq_next->rq_prev = thrd->rq_prev;
    }
    else {
        rq_tail = thrd->rq_prev;
    }

    thrd->rq_next = nullptr;
    thrd->rq_prev = nullptr;
}

// A thread woken up before it got off its
// hart is still locked by that hart and is skipped
static struct thread* rq_take_nolock(struct hart_local* restrict hart)
{
    for(struct thread* thrd = rq_head; thrd; thrd = thrd->rq_next) {
        if(thrd->pinned_hart && thrd->pinned_hart != hart) {
            continue;
        }

        if(raw_spinlock_try_acquire(&thrd->hart_lock)) {
            rq_remove_nolock(thrd);
            return thrd;
        }
    }

    return nullptr;
}

static __always_inline __nodiscard bool hart_is_idle(const struct hart_local* restrict hart)
{
    unsigned short flags = atomic_load_n(&hart->flags, ATOMIC_RELAXED);

    return HART_FLAG_ONLINE == (flags & (HART_FLAG_ONLINE | HART_FLAG_ACTIVE));
}

static void kick_idle_hart(const struct thread* restrict thrd)
{
    if(thrd->pinned_hart) {
        if(hart_is_idle(thrd->pinned_hart)) {
            hart_ipi(thrd->pinned_hart, sched_ireq_number);
        }

        return;
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
        struct hart_local* hart = &harts[i];

        if(hart_is_idle(hart)) {
            hart_ipi(hart, sched_ireq_number);
            return;
        }
    }
}

static void wake_expired(void)
{
    if(atomic_load(&sleep_head, ATOMIC_RELAXED)) {
        uint64_t now = clocksrc_nsec();

        raw_spinlock_acquire(&sleep_lock);

        while(sleep_head && sleep_head->wake_deadline <= now) {
            struct thread* thrd = sleep_head;
            sleep_head = thrd->wake_next;
            thrd->wake_next = nullptr;
            sched_enqueue(thrd);
        }

        raw_spinlock_release(&sleep_lock);
    }
}

static void reap_thread(struct thread* restrict thrd)
{
    if(thrd->proc) {
        process_thread_reaped(thrd->proc, thrd);
    }

    kstack_free(thrd->kstack_top);
    kfree(thrd->events);
    slab_free(thrd);
}

static void sched_ireq_handler(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    clockevt_stop();

    // Counters as narrow as the 24-bit ACPI PM timer
    // wrap within seconds; this call kind of shakes the
    // timer a little without resetting the actual time
    // so that if there is any wrapping, it's accounted for
    clocksrc_update();

    wake_expired();

    struct hart_local* hart = this_hart();

    // The thread that died here last time is off its
    // stack and address space by now; its process's
    // address space only goes with its last thread
    if(hart->dead_thread) {
        reap_thread(hart->dead_thread);
        hart->dead_thread = nullptr;
    }

    struct thread* curr = hart->curr_thread;

    // preempt_enable switches away once the last spinlock is
    // released; a blocking or dying thread has none to hold
    if(curr != hart->idle_thread && curr->state == THREAD_RUNNING && curr->preempt_count > 0) {
        curr->need_resched = true;
        clockevt_oneshot(sched_ireq_number, curr->timeslice);
        return;
    }

    curr->need_resched = false;

    raw_spinlock_acquire(&sched_lock);

    struct thread* next = rq_take_nolock(hart);

    // The handler runs on curr's stack, so curr
    // may only be handed over once sched_enter is off it
    spinlock_type* curr_hart_lock = nullptr;

    if(curr != hart->idle_thread) {
        raw_spinlock_release(&curr->yield_await);

        if(curr->state == THREAD_DEAD) {
            hart->dead_thread = curr;
        }
        else if(next == nullptr && curr->state != THREAD_BLOCKED) {
            // Nothing else to run: keep going, even
            // if curr was woken up while blocking
            if(curr->state == THREAD_RUNNABLE) {
                rq_remove_nolock(curr);
                curr->state = THREAD_RUNNING;
            }

            raw_spinlock_release(&sched_lock);
            clockevt_oneshot(sched_ireq_number, curr->timeslice);
            return;
        }
        else {
            memcpy(&curr->ctx, ctx, sizeof(struct hart_context));
            arch_thread_ctx_save(curr);

            if(curr->state == THREAD_RUNNING) {
                curr->state = THREAD_RUNNABLE;
                rq_push_nolock(curr);
            }

            curr_hart_lock = &curr->hart_lock;
        }
    }

    if(next) {
        next->state = THREAD_RUNNING;
    }

    raw_spinlock_release(&sched_lock);

    if(next == nullptr) {
        hart->flags &= ~HART_FLAG_ACTIVE;
        hart->curr_thread = hart->idle_thread;
        arch_thread_ctx_load(hart->idle_thread);
        clockevt_oneshot(sched_ireq_number, SCHED_TIMESLICE);
        sched_enter(&hart->idle_thread->ctx, curr_hart_lock);
    }

    hart->flags |= HART_FLAG_ACTIVE;
    next->hart = hart;
    hart->curr_thread = next;

    arch_thread_ctx_load(next);
    clockevt_oneshot(sched_ireq_number, next->timeslice);

    sched_enter(&next->ctx, curr_hart_lock);
}

static void sched_idle(void* restrict arg)
{
    for(;;) {
        enable_interrupts();
        hart_stop();
    }
}

static struct thread* alloc_thread_user(struct process* restrict proc)
{
    struct thread* thrd = slab_alloc(sizeof(struct thread));

    if(thrd == nullptr) {
        return nullptr;
    }

    void* kstack_top = kstack_alloc();

    if(kstack_top == nullptr) {
        slab_free(thrd);
        return nullptr;
    }

    thrd->self = thrd;
    thrd->state = THREAD_BLOCKED;
    thrd->timeslice = SCHED_TIMESLICE;
    thrd->proc = proc;
    thrd->rq_next = nullptr;
    thrd->rq_prev = nullptr;
    thrd->preempt_count = 0;
    thrd->need_resched = false;
    thrd->pinned_hart = nullptr;
    thrd->wake_next = nullptr;
    thrd->hart_lock = SPINLOCK_INIT;
    thrd->yield_await = SPINLOCK_INIT;
    thrd->kstack_top = kstack_top;

    return thrd;
}

static void attach_thread_user(struct process* restrict proc, struct thread* restrict thrd)
{
    process_incref(proc); // The thread keeps its process alive until it is reaped

    bool intr_state = spinlock_acquire_intr(&proc->lock);

    thrd->tnext = proc->threads;

    if(proc->threads) {
        proc->threads->tprev = thrd;
    }

    proc->threads = thrd;
    proc->num_threads += 1;

    spinlock_release_intr(&proc->lock, intr_state);
}

struct thread* create_thread_kern(thread_entry_kern entry, void* restrict arg)
{
    struct thread* thrd = slab_alloc(sizeof(struct thread));

    if(thrd == nullptr) {
        return nullptr;
    }

    void* kstack_top = kstack_alloc();

    if(kstack_top == nullptr) {
        slab_free(thrd);
        return nullptr;
    }

    thrd->self = thrd;
    thrd->state = THREAD_BLOCKED;
    thrd->timeslice = SCHED_TIMESLICE;
    thrd->proc = nullptr;
    thrd->rq_next = nullptr;
    thrd->rq_prev = nullptr;
    thrd->preempt_count = 0;
    thrd->need_resched = false;
    thrd->pinned_hart = nullptr;
    thrd->wake_next = nullptr;
    thrd->hart_lock = SPINLOCK_INIT;
    thrd->yield_await = SPINLOCK_INIT;
    thrd->kstack_top = kstack_top;

    arch_thread_setup_kern(thrd, entry, arg, kstack_top);

    return thrd;
}

struct thread* create_thread_kern_enq(thread_entry_kern entry, void* restrict arg)
{
    struct thread* thrd = create_thread_kern(entry, arg);

    if(thrd == nullptr) {
        return nullptr;
    }

    sched_enqueue(thrd);

    return thrd;
}

struct thread* create_thread_user(struct process* restrict proc, uintptr_t entry, uintptr_t stack_top)
{
    struct thread* thrd = alloc_thread_user(proc);

    if(thrd) {
        arch_thread_setup_user(thrd, (void*)(entry), (void*)(stack_top), thrd->kstack_top, proc->vms);
        attach_thread_user(proc, thrd);
    }

    return thrd;
}

struct thread* create_thread_fork(struct process* restrict proc, const struct hart_context* restrict ctx)
{
    struct thread* thrd = alloc_thread_user(proc);

    if(thrd) {
        arch_thread_setup_fork(thrd, ctx, thrd->kstack_top, proc->vms);
        attach_thread_user(proc, thrd);
    }

    return thrd;
}

struct thread* create_thread_user_enq(struct process* restrict proc, uintptr_t entry, uintptr_t stack_top)
{
    struct thread* thrd = create_thread_user(proc, entry, stack_top);

    if(thrd == nullptr) {
        return nullptr;
    }

    sched_enqueue(thrd);

    return thrd;
}

void sched_enqueue(struct thread* restrict thrd)
{
    bool intr_state = spinlock_acquire_intr(&sched_lock);

    if(thrd->state != THREAD_BLOCKED) {
        spinlock_release_intr(&sched_lock, intr_state);
        return;
    }

    thrd->state = THREAD_RUNNABLE;
    rq_push_nolock(thrd);

    spinlock_release_intr(&sched_lock, intr_state);

    kick_idle_hart(thrd);
}

void sched_dequeue(struct thread* restrict thrd)
{
    bool intr_state = spinlock_acquire_intr(&sched_lock);

    if(thrd->state == THREAD_RUNNABLE) {
        rq_remove_nolock(thrd);
    }

    if(thrd->state != THREAD_DEAD) {
        thrd->state = THREAD_BLOCKED;
    }

    spinlock_release_intr(&sched_lock, intr_state);
}

void sched_wake_at(struct thread* restrict thrd, uint64_t deadline)
{
    bool intr_state = spinlock_acquire_intr(&sleep_lock);

    struct thread** link = &sleep_head;

    while(*link && (*link)->wake_deadline <= deadline) {
        link = &(*link)->wake_next;
    }

    thrd->wake_deadline = deadline;
    thrd->wake_next = *link;
    *link = thrd;

    spinlock_release_intr(&sleep_lock, intr_state);
}

void sched_wake_cancel(struct thread* restrict thrd)
{
    bool intr_state = spinlock_acquire_intr(&sleep_lock);

    for(struct thread** link = &sleep_head; *link; link = &(*link)->wake_next) {
        if(*link == thrd) {
            *link = thrd->wake_next;
            thrd->wake_next = nullptr;
            break;
        }
    }

    spinlock_release_intr(&sleep_lock, intr_state);
}

void __noreturn sched_terminate(void)
{
    disable_interrupts();

    struct thread* thrd = this_thread();

    raw_spinlock_acquire(&sched_lock);

    if(thrd->state == THREAD_RUNNABLE) {
        rq_remove_nolock(thrd);
    }

    thrd->state = THREAD_DEAD;

    raw_spinlock_release(&sched_lock);

    struct hart_local* hart = this_hart();
    hart_ipi(hart, sched_ireq_number);

    enable_interrupts();

    for(;;) {
        hart_stop();
    }

    unreachable();
}

void sched_yield(void)
{
    struct thread* thrd = this_thread();

    // The switch is a self-IPI, which never
    // arrives with interrupts disabled
    kassert(interrupt_state());
    kassert(thrd->preempt_count == 0);

    disable_interrupts();

    struct hart_local* hart = this_hart();

    raw_spinlock_acquire(&thrd->yield_await);

    hart_ipi(hart, sched_ireq_number);

    enable_interrupts();

    raw_spinlock_acquire(&thrd->yield_await);
    raw_spinlock_release(&thrd->yield_await);
}

void preempt_disable(void)
{
    struct thread* thrd = this_thread();

    thrd->preempt_count += 1;
}

void preempt_enable(void)
{
    struct thread* thrd = this_thread();

    kassert(thrd->preempt_count > 0);

    thrd->preempt_count -= 1;

    // With interrupts disabled the next tick picks it up instead
    if(thrd->preempt_count == 0 && thrd->need_resched && interrupt_state()) {
        sched_yield();
    }
}

static void sched_init(void)
{
    sched_ireq_number = add_ireq_handler(UINT_MAX, &sched_ireq_handler, NULL);

    if(sched_ireq_number == UINT_MAX) {
        panic("sched: out of ireq vectors");
        unreachable();
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
        if(harts[i].bringup == HART_BRINGUP_PARK) {
            continue;
        }

        harts[i].idle_thread = create_thread_kern(&sched_idle, nullptr);

        if(harts[i].idle_thread == nullptr) {
            panic("sched: out of memory");
            unreachable();
        }

        harts[i].idle_thread->hart = &harts[i];
        harts[i].curr_thread = harts[i].idle_thread;
    }
}

initcall_bsp_multicore(sched_init, INIT_LATE);

static void sched_start(void)
{
    hart_ipi(HART_IPI_EVERYONE, sched_ireq_number);
}

initcall_bsp_late(sched_start, INIT_FIRST);
