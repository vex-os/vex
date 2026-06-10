#include <vex.arch/hart.h>
#include <vex.arch/intr.h>
#include <vex/atomic.h>
#include <vex/clocksrc.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/process.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/slab.h>
#include <vex/thread.h>
#include <vex/vmm.h>

static void push_tail(struct hart_local* restrict hart, struct thread* restrict thrd)
{
    thrd->runq_next = nullptr;

    if(hart->runq_head == nullptr) {
        hart->runq_head = thrd;
        return;
    }

    struct thread* tail = hart->runq_head;

    while(tail->runq_next) {
        tail = tail->runq_next;
    }

    tail->runq_next = thrd;
}

static intptr_t __noreturn idle_thread(void* restrict ctxdata)
{
    for(;;) {
        sched_idle();
    }

    unreachable();
}

void sched_idle(void)
{
    enable_interrupts();
    stop_hart();
    disable_interrupts();
}

void sched_preempt(void)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    if(hart->dead_head) {
        vmm_switch(&sys_pagemap);
    }

    while(hart->dead_head) {
        struct thread* dead = hart->dead_head;
        hart->dead_head = dead->runq_next;
        dead->runq_next = nullptr;

        if(dead->owner) {
            if(1 == atomic_fetch_sub(&dead->owner->live_count, 1, ATOMIC_RELAXED)) {
                process_destroy(dead->owner);
            }
        }

        thread_destroy(dead);
    }

    uint64_t now = clocksrc_nsec();

    struct thread* prev = hart->curr_thread;
    struct thread* next = nullptr;

    if(prev != hart->idle_thread && prev) {
        if(prev->state == THREAD_EXEC) {
            prev->state = THREAD_WAIT;
            push_tail(hart, prev);
        }
        else if(prev->state == THREAD_DEAD || prev->state == THREAD_SEGV) {
            // Thread was killed while it was curr_thread (not via thread_exit_handler).
            // Add it to dead_head so it gets reaped on the next sched_preempt call.
            prev->runq_next = hart->dead_head;
            hart->dead_head = prev;
        }
    }

    struct thread* rq_item = hart->runq_head;
    struct thread* rq_prev = nullptr;

    while(rq_item) {
        struct thread* rq_next = rq_item->runq_next;

        if(rq_item->state == THREAD_DEAD || rq_item->state == THREAD_SEGV) {
            if(rq_prev) {
                rq_prev->runq_next = rq_next;
            }
            else {
                hart->runq_head = rq_next;
            }

            rq_item->runq_next = hart->dead_head;

            hart->dead_head = rq_item;
            rq_item = rq_next;

            continue;
        }

        if(rq_item->deadline == 0 || now >= rq_item->deadline) {
            next = rq_item;

            if(rq_prev) {
                rq_prev->runq_next = rq_next;
            }
            else {
                hart->runq_head = rq_next;
            }

            break;
        }

        rq_prev = rq_item;
        rq_item = rq_next;
    }

    if(next == nullptr) {
        next = hart->idle_thread;
    }

    if(next == prev) {
        restore_interrupts(intr_state);
        return;
    }

    next->state = THREAD_EXEC;
    hart->curr_thread = next;

    if(next->owner) {
        vmm_switch(next->owner->vm);
    }
    else {
        vmm_switch(&sys_pagemap);
    }

    pr_verbose("thread_switch: %p -> %p", prev, next);
    thread_switch_hook(hart, next);
    thread_switch(prev, next);

    restore_interrupts(intr_state);
}

void sched_yield(void)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct thread* self = hart->curr_thread;
    self->state = THREAD_WAIT;
    push_tail(hart, self);

    hart->curr_thread = hart->idle_thread;
    hart->idle_thread->state = THREAD_EXEC;

    if(self->owner) {
        vmm_switch(&sys_pagemap);
    }

    pr_verbose("thread_switch: %p -> %p [idle]", self, hart->idle_thread);
    thread_switch_hook(hart, hart->idle_thread);
    thread_switch(self, hart->idle_thread);

    restore_interrupts(intr_state);
}

void sched_nsleep(uint64_t nsec)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct thread* self = hart->curr_thread;
    self->state = THREAD_WAIT;
    self->deadline = nsec + clocksrc_nsec();
    push_tail(hart, self);

    hart->curr_thread = hart->idle_thread;
    hart->idle_thread->state = THREAD_EXEC;

    if(self->owner) {
        vmm_switch(&sys_pagemap);
    }

    pr_verbose("thread_switch: %p -> %p [idle]", self, hart->idle_thread);
    thread_switch_hook(hart, hart->idle_thread);
    thread_switch(self, hart->idle_thread);

    restore_interrupts(intr_state);
}

void sched_usleep(uint64_t usec)
{
    sched_nsleep(usec * 1000);
}

void sched_msleep(uint64_t msec)
{
    sched_nsleep(msec * 1000000);
}

void sched_enqueue(struct thread* restrict thrd)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    thrd->state = THREAD_WAIT;
    push_tail(hart, thrd);

    restore_interrupts(intr_state);
}

static void init_sched(void)
{
    struct thread* idle = thread_create_kern(nullptr, &idle_thread, nullptr);

    if(idle == nullptr) {
        panic("sched: out of memory");
    }

    idle->state = THREAD_EXEC;

    struct hart_local* hart = this_hart();
    hart->idle_thread = idle;
    hart->curr_thread = idle;
}

initcall_smp_late(init_sched, INIT_LAST);
