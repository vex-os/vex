#include <vex.arch/ireq.h>
#include <vex/atomic.h>
#include <vex/clockevt.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kmalloc.h>
#include <vex/limits.h>
#include <vex/panic.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/spinlock.h>
#include <vex/string.h>

#ifndef SCHED_QUEUE_SIZE
#define SCHED_QUEUE_SIZE 65536
#endif

#ifndef SCHED_TIMESLICE
#define SCHED_TIMESLICE 5000000 // 5ms
#endif

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
        // TODO: save cr3, GS/FS bases, FPU state

        curr->state = THREAD_RUNNABLE;
        spinlock_release(&curr->hart_lock);
    }

    if(next == nullptr) {
        hart->flags &= ~HART_FLAG_ACTIVE;
        hart->curr_thread = hart->idle_thread;
        // TODO (Stage 4): switch to idle thread context (CR3, GS)
        clockevt_oneshot(sched_ireq_number, SCHED_TIMESLICE);
        sched_enter(&hart->idle_thread->ctx);
    }

    next->state = THREAD_RUNNING;
    next->hart = hart;
    hart->curr_thread = next;

    // TODO (Stage 4): restore CR3, GS/FS bases, FPU; update TSS kernel/pf stacks
    clockevt_oneshot(sched_ireq_number, next->timeslice);

    sched_enter(&next->ctx);
}

static void sched_idle(void)
{
    for(;;) {
        enable_interrupts();
        hart_stop();
    }
}

static struct thread* create_idle_thread(void)
{
    struct thread* thrd = kmalloc(sizeof(struct thread));
    void* stack = pslab_alloc_virt();

    if(thrd == nullptr || stack == nullptr) {
        panic("sched: out of memory");
    }

    thrd->self = thrd;
    thrd->state = THREAD_RUNNABLE;
    thrd->timeslice = 0;
    thrd->owner = nullptr;
    thrd->enqueued = false;
    thrd->hart_lock = SPINLOCK_INIT;
    thrd->yield_await = SPINLOCK_INIT;

    // FIXME: these shouldn't be used
    // outside of arch/x86_64/kernel!!
    thrd->ctx.x86_ds = 0x30;
    thrd->ctx.x86_es = 0x30;
    thrd->ctx.x86_rip = (uint64_t)sched_idle;
    thrd->ctx.x86_cs = 0x28;
    thrd->ctx.x86_rflags = 0x202;
    thrd->ctx.x86_rsp = (uint64_t)stack + 0x1000;
    thrd->ctx.x86_ss = 0x30;

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

            for(unsigned long j = 0; j < num_harts; ++j) {
                if(0 == (harts[j].flags & HART_FLAG_ACTIVE)) {
                    hart_ipi(&harts[j], sched_ireq_number);
                    break;
                }
            }
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
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
        harts[i].idle_thread = create_idle_thread();
        harts[i].idle_thread->hart = &harts[i];
        harts[i].curr_thread = harts[i].idle_thread;
    }
}

initcall_bsp_multicore(sched_init, INIT_LATE);

static void sched_start(void)
{
    clockevt_oneshot(sched_ireq_number, SCHED_TIMESLICE);
}

initcall_bsp_late(sched_start, INIT_FIRST);
