#include <vex.arch/ireq.h>
#include <vex/atomic.h>
#include <vex/clockevt.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/limits.h>
#include <vex/panic.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/thread.h>

#ifndef SCHED_QUEUE_SIZE
#define SCHED_QUEUE_SIZE 65536
#endif

#ifndef SCHED_TIMESLICE
#define SCHED_TIMESLICE 5000000
#endif

// Threads and processes are something that gets
// allocated quite a bit of times across the kernel's
// lifetime so it makes sense to use a fast allocator
// for them; the slab allocator is O(1) for both alloc and free but imposes a size limit
static_assert(sizeof(struct thread) <= (PAGE_SIZE / 2));

static struct thread* run_queue[SCHED_QUEUE_SIZE];
static unsigned sched_ireq_number;

static struct thread* get_next_thread(void)
{
    struct hart_local* hart = this_hart();
    unsigned long origin = hart->last_rq_index;

    if(origin >= SCHED_QUEUE_SIZE) {
        origin = 0;
    }

    unsigned long index = origin + 1;

    for(;;) {
        if(index >= SCHED_QUEUE_SIZE) {
            index = 0;
        }

        struct thread* thrd = atomic_load_n(&run_queue[index], ATOMIC_RELAXED);

        if(thrd && spinlock_try_acquire(&thrd->hart_lock)) {
            hart->last_rq_index = index;
            return thrd;
        }

        if(index == origin) {
            break;
        }

        index += 1;
    }

    hart->last_rq_index = index;
    return nullptr;
}

static void sched_ireq_handler(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    clockevt_stop();

    struct hart_local* hart = this_hart();
    struct thread* curr = hart->curr_thread;
    struct thread* next = get_next_thread();

    if(curr != hart->idle_thread) {
        spinlock_release(&curr->yield_await);

        if(next == nullptr && curr->enqueued) {
            clockevt_oneshot(sched_ireq_number, curr->timeslice);
            return;
        }

        memcpy(&curr->ctx, ctx, sizeof(struct hart_context));
        arch_thread_ctx_save(curr);

        curr->state = THREAD_RUNNABLE;
        spinlock_release(&curr->hart_lock);
    }

    if(next == nullptr) {
        hart->flags &= ~HART_FLAG_ACTIVE;
        hart->curr_thread = hart->idle_thread;
        arch_thread_ctx_load(hart->idle_thread);
        clockevt_oneshot(sched_ireq_number, SCHED_TIMESLICE);
        sched_enter(&hart->idle_thread->ctx);
    }

    next->state = THREAD_RUNNING;
    next->hart = hart;
    hart->curr_thread = next;

    arch_thread_ctx_load(next);
    clockevt_oneshot(sched_ireq_number, next->timeslice);

    sched_enter(&next->ctx);
}

static void sched_idle(void* restrict arg)
{
    for(;;) {
        enable_interrupts();
        hart_stop();
    }
}

struct thread* create_thread_kern(thread_entry_kern entry, void* restrict arg)
{
    struct thread* thrd = slab_alloc(sizeof(struct thread));

    if(thrd == nullptr) {
        return nullptr;
    }

    uint8_t* kstack = pslab_alloc_virt();

    if(kstack == nullptr) {
        slab_free(thrd);
        return nullptr;
    }

    thrd->self = thrd;
    thrd->state = THREAD_RUNNABLE;
    thrd->timeslice = SCHED_TIMESLICE;
    thrd->proc = nullptr;
    thrd->enqueued = false;
    thrd->hart_lock = SPINLOCK_INIT;
    thrd->yield_await = SPINLOCK_INIT;

    arch_thread_setup_kern(thrd, entry, arg, kstack + PAGE_SIZE);

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

void sched_enqueue(struct thread* restrict thrd)
{
    if(thrd->enqueued) {
        return;
    }

    struct thread* expt = nullptr;

    for(unsigned long i = 0; i < SCHED_QUEUE_SIZE; ++i) {
        if(atomic_cmpxchg(&run_queue[i], &expt, thrd, ATOMIC_ACQ_REL, ATOMIC_RELAXED)) {
            thrd->enqueued = true;
            hart_ipi(HART_IPI_EXCEPT_ME, sched_ireq_number);
            return;
        }

        expt = nullptr;
    }

    // FIXME: there should be a better
    // way to deal with the runqueue filling up;
    // maybe use krealloc for this specific purpose?
    panic("sched: run queue is full");
}

void sched_dequeue(struct thread* restrict thrd)
{
    if(!thrd->enqueued) {
        return;
    }

    for(unsigned long i = 0; i < SCHED_QUEUE_SIZE; ++i) {
        struct thread* expected = thrd;

        if(atomic_cmpxchg(&run_queue[i], &expected, nullptr, ATOMIC_ACQ_REL, ATOMIC_RELAXED)) {
            thrd->enqueued = false;
            return;
        }
    }
}

void __noreturn sched_terminate(void)
{
    disable_interrupts();

    struct thread* thrd = this_thread();

    thrd->state = THREAD_DEAD;
    sched_dequeue(thrd);

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
    bool intr_state = disable_interrupts();

    struct thread* thrd = this_thread();
    struct hart_local* hart = this_hart();

    spinlock_acquire(&thrd->yield_await);

    hart_ipi(hart, sched_ireq_number);

    restore_interrupts(intr_state);

    spinlock_acquire(&thrd->yield_await);
    spinlock_release(&thrd->yield_await);
}

static void sched_init(void)
{
    sched_ireq_number = add_ireq_handler(UINT_MAX, &sched_ireq_handler, NULL);

    if(sched_ireq_number == UINT_MAX) {
        panic("sched: out of ireq vectors");
        unreachable();
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
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
