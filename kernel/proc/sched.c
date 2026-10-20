#include <arch/irq.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/clockevt.h>
#include <vex/clocksrc.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/irq.h>
#include <vex/kmalloc.h>
#include <vex/kstack.h>
#include <vex/limits.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/preempt.h>
#include <vex/process.h>
#include <vex/sched.h>
#include <vex/signal.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vms.h>

#define SCHED_TIMESLICE_NS (1000 * UINT64_C(CONFIG_SCHED_TIMESLICE))
#define THREAD_SIZE_BUDGET 256

static_assert(sizeof(thread_t) <= THREAD_SIZE_BUDGET);

typedef struct runqueue_s runqueue_t;

struct runqueue_s {
    spinlock_t lock;
    thread_t* head;
    thread_t* tail;
    unsigned long num_queued; // Read without the lock by idle harts and thieves
};

static runqueue_t runqueues[MAX_HARTS];
static unsigned sched_irq_number;
static spinlock_t sleep_lock = SPINLOCK_INIT;
static thread_t* sleep_head;

static void rq_push_nolock(runqueue_t* restrict runqueue, thread_t* restrict thread)
{
    thread->runqueue = runqueue;
    thread->runqueue_next = nullptr;
    thread->runqueue_prev = runqueue->tail;

    if(runqueue->tail) {
        runqueue->tail->runqueue_next = thread;
    }
    else {
        runqueue->head = thread;
    }

    runqueue->tail = thread;

    atomic_fetch_add(&runqueue->num_queued, 1, ATOMIC_SEQ_CST);
}

static void rq_remove_nolock(runqueue_t* restrict runqueue, thread_t* restrict thread)
{
    if(thread->runqueue_prev) {
        thread->runqueue_prev->runqueue_next = thread->runqueue_next;
    }
    else {
        runqueue->head = thread->runqueue_next;
    }

    if(thread->runqueue_next) {
        thread->runqueue_next->runqueue_prev = thread->runqueue_prev;
    }
    else {
        runqueue->tail = thread->runqueue_prev;
    }

    thread->runqueue = nullptr;
    thread->runqueue_next = nullptr;
    thread->runqueue_prev = nullptr;

    atomic_fetch_sub(&runqueue->num_queued, 1, ATOMIC_RELAXED);
}

static void rq_unlink(thread_t* restrict thread)
{
    runqueue_t* runqueue = thread->runqueue;

    raw_spinlock_acquire(&runqueue->lock);
    rq_remove_nolock(runqueue, thread);
    raw_spinlock_release(&runqueue->lock);
}

static thread_t* rq_take_nolock(runqueue_t* restrict runqueue, hart_local_t* restrict hart)
{
    for(thread_t* thread = runqueue->head; thread; thread = thread->runqueue_next) {
        if(thread->bound_hart && thread->bound_hart != hart) {
            continue;
        }

        if(!raw_spinlock_try_acquire(&thread->hart_lock)) {
            continue;
        }

        if(!raw_spinlock_try_acquire(&thread->sched_lock)) {
            raw_spinlock_release(&thread->hart_lock);
            continue;
        }

        rq_remove_nolock(runqueue, thread);
        thread->state = THREAD_RUNNING;

        raw_spinlock_release(&thread->sched_lock);

        return thread;
    }

    return nullptr;
}

// https://files.catbox.moe/hxlgm4.png
static thread_t* rq_steal(hart_local_t* restrict hart)
{
    for(unsigned long i = 1; i < num_harts; ++i) {
        hart_local_t* victim = &harts[(hart->index + i) % num_harts];
        runqueue_t* runqueue = &runqueues[victim->index];

        if(0 == atomic_load_n(&runqueue->num_queued, ATOMIC_RELAXED)) {
            continue;
        }

        if(!raw_spinlock_try_acquire(&runqueue->lock)) {
            continue;
        }

        thread_t* thread = rq_take_nolock(runqueue, hart);

        raw_spinlock_release(&runqueue->lock);

        if(thread) {
            return thread;
        }
    }

    return nullptr;
}

static __always_inline __nodiscard bool hart_is_idle(const hart_local_t* restrict hart)
{
    unsigned short flags = atomic_load_n(&hart->flags, ATOMIC_SEQ_CST);

    return HART_FLAG_ONLINE == (flags & (HART_FLAG_ONLINE | HART_FLAG_ACTIVE));
}

static hart_local_t* choose_hart(const thread_t* restrict thread)
{
    if(thread->bound_hart) {
        return thread->bound_hart;
    }

    hart_local_t* last = atomic_load(&thread->last_hart, ATOMIC_RELAXED);

    if(last) {
        if(SPINLOCK_LOCK == atomic_load(&thread->hart_lock, ATOMIC_ACQUIRE)) {
            return last;
        }

        if(hart_is_idle(last)) {
            return last;
        }
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
        hart_local_t* hart = &harts[i];

        if(hart_is_idle(hart)) {
            return hart;
        }
    }

    // TODO: balance queues between busy harts on the tick,
    // behind a Kconfig option; until then a busy hart's queue
    // only drains into harts that run out of their own work
    if(last) {
        return last;
    }

    return this_hart();
}

static void kick_hart(hart_local_t* restrict target, const thread_t* restrict thread)
{
    if(hart_is_idle(target)) {
        hart_ipi(target, sched_irq_number);
        return;
    }

    if(thread->bound_hart) {
        return;
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
        hart_local_t* hart = &harts[i];

        if(hart_is_idle(hart)) {
            hart_ipi(hart, sched_irq_number);
            return;
        }
    }
}

static void wake_expired(void)
{
    if(atomic_load(&sleep_head, ATOMIC_RELAXED)) {
        uint64_t now = clock_source_now_ns();

        raw_spinlock_acquire(&sleep_lock);

        while(sleep_head && sleep_head->wake_deadline <= now) {
            thread_t* thread = sleep_head;
            sleep_head = thread->wake_next;
            thread->wake_next = nullptr;
            sched_enqueue(thread);
        }

        raw_spinlock_release(&sleep_lock);
    }
}

static void reap_thread(thread_t* restrict thread)
{
    if(thread->process) {
        process_thread_reaped(thread->process, thread);
    }

    cred_decref(thread->credentials);
    signal_flush(thread->sig_queue);
    kstack_free(thread->kstack_top);
    kfree(thread->events);
    slab_free(thread);
}

static void sched_irq_handler(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    clock_event_stop();

    // Counters as narrow as the 24-bit ACPI PM timer
    // wrap within seconds; this call kind of shakes the
    // timer a little without resetting the actual time
    // so that if there is any wrapping, it's accounted for
    clock_source_update();

    wake_expired();

    hart_local_t* hart = this_hart();

    // The thread that died here last time is off its
    // stack and address space by now; its process's
    // address space only goes with its last thread
    if(hart->dead_thread) {
        reap_thread(hart->dead_thread);
        hart->dead_thread = nullptr;
    }

    thread_t* curr = hart->curr_thread;

    // preempt_enable switches away once the last spinlock is
    // released; a blocking or dying thread has none to hold
    if(curr != hart->idle_thread && curr->state == THREAD_RUNNING && curr->preempt_count > 0) {
        curr->need_resched = true;
        clock_event_oneshot(sched_irq_number, curr->timeslice);
        return;
    }

    curr->need_resched = false;

    runqueue_t* runqueue = &runqueues[hart->index];

    if(curr != hart->idle_thread) {
        raw_spinlock_release(&curr->yield_await);
        raw_spinlock_acquire(&curr->sched_lock);
    }

    raw_spinlock_acquire(&runqueue->lock);

    thread_t* next = rq_take_nolock(runqueue, hart);

    // The handler runs on curr's stack, so curr
    // may only be handed over once sched_enter is off it
    spinlock_t* curr_hart_lock = nullptr;

    if(curr != hart->idle_thread) {
        if(curr->state == THREAD_DEAD) {
            hart->dead_thread = curr;
        }
        else if(next == nullptr && curr->state != THREAD_BLOCKED) {
            // Nothing else to run: keep going, even
            // if curr was woken up while blocking
            if(curr->state == THREAD_RUNNABLE) {
                kassert(curr->runqueue == runqueue);
                rq_remove_nolock(runqueue, curr);
                curr->state = THREAD_RUNNING;
            }

            raw_spinlock_release(&runqueue->lock);
            raw_spinlock_release(&curr->sched_lock);
            clock_event_oneshot(sched_irq_number, curr->timeslice);
            return;
        }
        else {
            memcpy(thread_context(curr), ctx, sizeof(hart_context_t));
            arch_thread_ctx_save(curr);

            if(curr->state == THREAD_RUNNING) {
                curr->state = THREAD_RUNNABLE;
                rq_push_nolock(runqueue, curr);
            }

            curr_hart_lock = &curr->hart_lock;
        }
    }

    raw_spinlock_release(&runqueue->lock);

    if(curr != hart->idle_thread) {
        raw_spinlock_release(&curr->sched_lock);
    }

    // Only a hart that would otherwise idle steals
    if(next == nullptr) {
        next = rq_steal(hart);
    }

    if(next == nullptr) {
        atomic_fetch_and(&hart->flags, (unsigned short)(~HART_FLAG_ACTIVE), ATOMIC_SEQ_CST);
        hart->curr_thread = hart->idle_thread;
        arch_thread_ctx_load(hart->idle_thread);
        clock_event_oneshot(sched_irq_number, SCHED_TIMESLICE_NS);
        sched_enter(thread_context(hart->idle_thread), curr_hart_lock);
    }

    atomic_fetch_or(&hart->flags, HART_FLAG_ACTIVE, ATOMIC_SEQ_CST);
    atomic_store(&next->last_hart, hart, ATOMIC_RELAXED);
    hart->curr_thread = next;

    arch_thread_ctx_load(next);
    clock_event_oneshot(sched_irq_number, next->timeslice);

    sched_enter(thread_context(next), curr_hart_lock);
}

static void sched_idle(void* restrict arg)
{
    hart_local_t* hart = this_hart();
    runqueue_t* runqueue = &runqueues[hart->index];

    for(;;) {
        disable_interrupts();

        if(atomic_load_n(&runqueue->num_queued, ATOMIC_SEQ_CST)) {
            hart_ipi(hart, sched_irq_number);
        }

        enable_interrupts();
        hart_stop();
    }
}

static thread_t* alloc_thread_user(process_t* restrict process)
{
    thread_t* thread = slab_alloc(sizeof(thread_t));

    if(thread == nullptr) {
        return nullptr;
    }

    void* kstack_top = kstack_alloc();

    if(kstack_top == nullptr) {
        slab_free(thread);
        return nullptr;
    }

    thread->self = thread;
    thread->state = THREAD_BLOCKED;
    thread->timeslice = SCHED_TIMESLICE_NS;
    thread->process = process;
    thread->credentials = process_get_cred(process);
    thread->runqueue = nullptr;
    thread->runqueue_next = nullptr;
    thread->runqueue_prev = nullptr;
    thread->preempt_count = 0;
    thread->need_resched = false;
    thread->last_hart = nullptr;
    thread->bound_hart = nullptr;
    thread->wake_next = nullptr;
    thread->sched_lock = SPINLOCK_INIT;
    thread->hart_lock = SPINLOCK_INIT;
    thread->yield_await = SPINLOCK_INIT;
    thread->kstack_top = kstack_top;

    return thread;
}

static void attach_thread_user(process_t* restrict process, thread_t* restrict thread)
{
    process_incref(process); // The thread keeps its process alive until it is reaped

    bool intr_state = spinlock_acquire_intr(&process->lock);

    thread->process_next = process->threads;

    if(process->threads) {
        process->threads->process_prev = thread;
    }

    process->threads = thread;
    process->num_threads += 1;

    spinlock_release_intr(&process->lock, intr_state);
}

static hart_local_t* wake_locked(thread_t* restrict thread)
{
    if(thread->state != THREAD_BLOCKED) {
        return nullptr;
    }

    hart_local_t* target = choose_hart(thread);
    runqueue_t* runqueue = &runqueues[target->index];

    raw_spinlock_acquire(&runqueue->lock);

    thread->state = THREAD_RUNNABLE;
    rq_push_nolock(runqueue, thread);

    raw_spinlock_release(&runqueue->lock);

    return target;
}

thread_t* thread_create_kernel(kernel_thread_entry_t entry, void* restrict arg)
{
    thread_t* thread = slab_alloc(sizeof(thread_t));

    if(thread == nullptr) {
        return nullptr;
    }

    void* kstack_top = kstack_alloc();

    if(kstack_top == nullptr) {
        slab_free(thread);
        return nullptr;
    }

    thread->self = thread;
    thread->state = THREAD_BLOCKED;
    thread->timeslice = SCHED_TIMESLICE_NS;
    thread->process = nullptr;
    thread->credentials = &kernel_credentials;
    thread->runqueue = nullptr;
    thread->runqueue_next = nullptr;
    thread->runqueue_prev = nullptr;
    thread->preempt_count = 0;
    thread->need_resched = false;
    thread->last_hart = nullptr;
    thread->bound_hart = nullptr;
    thread->wake_next = nullptr;
    thread->sched_lock = SPINLOCK_INIT;
    thread->hart_lock = SPINLOCK_INIT;
    thread->yield_await = SPINLOCK_INIT;
    thread->kstack_top = kstack_top;

    cred_incref(&kernel_credentials);

    arch_thread_setup_kern(thread, entry, arg);

    return thread;
}

thread_t* thread_spawn_kernel(kernel_thread_entry_t entry, void* restrict arg)
{
    thread_t* thread = thread_create_kernel(entry, arg);

    if(thread == nullptr) {
        return nullptr;
    }

    sched_enqueue(thread);

    return thread;
}

thread_t* thread_create_user(process_t* restrict process, uintptr_t entry, uintptr_t stack_top)
{
    thread_t* thread = alloc_thread_user(process);

    if(thread) {
        arch_thread_setup_user(thread, (void*)(entry), (void*)(stack_top), process->vmspace);
        attach_thread_user(process, thread);
    }

    return thread;
}

thread_t* thread_create_fork(process_t* restrict process, const hart_context_t* restrict ctx)
{
    thread_t* thread = alloc_thread_user(process);

    if(thread) {
        arch_thread_setup_fork(thread, ctx, process->vmspace);
        attach_thread_user(process, thread);
    }

    return thread;
}

thread_t* thread_spawn_user(process_t* restrict process, uintptr_t entry, uintptr_t stack_top)
{
    thread_t* thread = thread_create_user(process, entry, stack_top);

    if(thread == nullptr) {
        return nullptr;
    }

    sched_enqueue(thread);

    return thread;
}

void sched_enqueue(thread_t* restrict thread)
{
    bool intr_state = spinlock_acquire_intr(&thread->sched_lock);

    hart_local_t* target = wake_locked(thread);

    spinlock_release_intr(&thread->sched_lock, intr_state);

    if(target) {
        kick_hart(target, thread);
    }
}

void sched_dequeue(thread_t* restrict thread, bool interruptible)
{
    bool intr_state = spinlock_acquire_intr(&thread->sched_lock);

    thread->wait_interrupt = interruptible;

    // Work posted before sched_kick could see the thread blocked
    // would never wake it, so such a wait does not start at all
    if(interruptible && atomic_load(&thread->pending_work, ATOMIC_ACQUIRE)) {
        spinlock_release_intr(&thread->sched_lock, intr_state);
        return;
    }

    if(thread->state == THREAD_RUNNABLE) {
        rq_unlink(thread);
    }

    if(thread->state != THREAD_DEAD) {
        thread->state = THREAD_BLOCKED;
    }

    spinlock_release_intr(&thread->sched_lock, intr_state);
}

void sched_kick(thread_t* restrict thread)
{
    bool intr_state = spinlock_acquire_intr(&thread->sched_lock);

    if(thread->state == THREAD_BLOCKED && thread->wait_interrupt) {
        hart_local_t* target = wake_locked(thread);

        spinlock_release_intr(&thread->sched_lock, intr_state);

        kick_hart(target, thread);
        return;
    }

    hart_local_t* target = nullptr;

    if(thread->state == THREAD_RUNNING) {
        target = atomic_load(&thread->last_hart, ATOMIC_RELAXED);
    }

    if(target && target != this_hart()) {
        hart_ipi(target, sched_irq_number);
    }

    spinlock_release_intr(&thread->sched_lock, intr_state);
}

void sched_wake_at(thread_t* restrict thread, uint64_t deadline)
{
    bool intr_state = spinlock_acquire_intr(&sleep_lock);

    thread_t** link = &sleep_head;

    while(*link && (*link)->wake_deadline <= deadline) {
        link = &(*link)->wake_next;
    }

    thread->wake_deadline = deadline;
    thread->wake_next = *link;
    *link = thread;

    spinlock_release_intr(&sleep_lock, intr_state);
}

void sched_wake_cancel(thread_t* restrict thread)
{
    bool intr_state = spinlock_acquire_intr(&sleep_lock);

    for(thread_t** link = &sleep_head; *link; link = &(*link)->wake_next) {
        if(*link == thread) {
            *link = thread->wake_next;
            thread->wake_next = nullptr;
            break;
        }
    }

    spinlock_release_intr(&sleep_lock, intr_state);
}

void __noreturn sched_terminate(void)
{
    disable_interrupts();

    thread_t* thread = this_thread();

    raw_spinlock_acquire(&thread->sched_lock);

    if(thread->state == THREAD_RUNNABLE) {
        rq_unlink(thread);
    }

    thread->state = THREAD_DEAD;

    raw_spinlock_release(&thread->sched_lock);

    hart_local_t* hart = this_hart();
    hart_ipi(hart, sched_irq_number);

    enable_interrupts();

    for(;;) {
        hart_stop();
    }

    unreachable();
}

void sched_yield(void)
{
    thread_t* thread = this_thread();

    // The switch is a self-IPI, which never
    // arrives with interrupts disabled
    kassert(interrupt_state());
    kassert(thread->preempt_count == 0);

    disable_interrupts();

    hart_local_t* hart = this_hart();

    raw_spinlock_acquire(&thread->yield_await);

    hart_ipi(hart, sched_irq_number);

    enable_interrupts();

    raw_spinlock_acquire(&thread->yield_await);
    raw_spinlock_release(&thread->yield_await);
}

void preempt_disable(void)
{
    thread_t* thread = this_thread();

    thread->preempt_count += 1;
}

void preempt_enable(void)
{
    thread_t* thread = this_thread();

    kassert(thread->preempt_count > 0);

    thread->preempt_count -= 1;

    // With interrupts disabled the next tick picks it up instead
    if(thread->preempt_count == 0 && thread->need_resched && interrupt_state()) {
        sched_yield();
    }
}

static void sched_init(void)
{
    sched_irq_number = irq_handler_add(UINT_MAX, &sched_irq_handler, NULL);

    if(sched_irq_number == UINT_MAX) {
        panic("sched: out of irq vectors");
        unreachable();
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
        if(harts[i].bringup == HART_BRINGUP_PARK) {
            continue;
        }

        harts[i].idle_thread = thread_create_kernel(&sched_idle, nullptr);

        if(harts[i].idle_thread == nullptr) {
            panic("sched: out of memory");
            unreachable();
        }

        harts[i].idle_thread->last_hart = &harts[i];
        harts[i].curr_thread = harts[i].idle_thread;
    }
}

initcall_bsp_multicore(sched_init, INIT_LATE);

static void sched_start(void)
{
    hart_ipi(HART_IPI_EVERYONE, sched_irq_number);
}

initcall_bsp_late(sched_start, INIT_FIRST);
